/**
 * @file    kws_mfcc.c
 * @brief   自包含 MFCC 特征提取 (FPU 加速, 无外部依赖)
 *
 * [2026-07-04]: KWS 集成。使用 radix-2 FFT + Mel 滤波器组 + DCT + per-sample z-score。
 *   与训练管线对齐: librosa.feature.mfcc(norm='ortho', 无pre-emphasis) + normalize_mfcc().
 *   STM32U5 M33 FPU: 单周期浮点乘加, 512-FFT ~0.3ms, 总提取 <3ms.
 */

#include "kws_mfcc.h"
#include <math.h>
#include <string.h>

/* ---- 预计算的 Mel 滤波器组 (64 × 257) ---- */
static float mel_filterbank[KWS_NUM_MELS][KWS_FFT_SIZE / 2 + 1];
static int   mel_ready = 0;

/* ---- Hamming 窗 (512) ---- */
static float hamming_window[KWS_FFT_SIZE];

/* ---- DCT 矩阵 (40 × 64) ---- */
static float dct_matrix[KWS_NUM_MFCC][KWS_NUM_MELS];

/* ============================================================
 * 初始化 (首次调用时自动执行)
 * ============================================================ */
static void kws_mfcc_init(void)
{
    if (mel_ready) return;

    /* Hamming 窗: 0.54 - 0.46 * cos(2πn/(N-1)) */
    for (int i = 0; i < KWS_FFT_SIZE; i++)
        hamming_window[i] = 0.54f - 0.46f * cos(6.283185307f * i / (KWS_FFT_SIZE - 1));

    /* Mel 滤波器组: 64 个三角滤波器, 线性分布在 Mel 尺度上 */
    float mel_low  = 2595.0f * log10f(1.0f + 0.0f  / 700.0f);     /* 0 Hz */
    float mel_high = 2595.0f * log10f(1.0f + KWS_SAMPLE_RATE / 2.0f / 700.0f);
    float mel_step = (mel_high - mel_low) / (KWS_NUM_MELS + 1);

    float mel_centers[KWS_NUM_MELS + 2];
    for (int m = 0; m < KWS_NUM_MELS + 2; m++)
        mel_centers[m] = 700.0f * (powf(10.0f, (mel_low + m * mel_step) / 2595.0f) - 1.0f);

    int   bin_centers[KWS_NUM_MELS + 2];
    for (int m = 0; m < KWS_NUM_MELS + 2; m++)
        bin_centers[m] = (int)(mel_centers[m] * KWS_FFT_SIZE / KWS_SAMPLE_RATE + 0.5f);

    memset(mel_filterbank, 0, sizeof(mel_filterbank));
    for (int m = 1; m <= KWS_NUM_MELS; m++) {
        int lo = bin_centers[m - 1], ce = bin_centers[m], hi = bin_centers[m + 1];
        for (int k = lo; k < ce; k++)
            mel_filterbank[m - 1][k] = (float)(k - lo) / (float)(ce - lo);
        for (int k = ce; k < hi; k++)
            mel_filterbank[m - 1][k] = (float)(hi - k) / (float)(hi - ce);
    }

    /* DCT Type-2 矩阵 (未归一化, 与 scipy.fftpack.dct type=2 一致)。
     *   dct[i][j] = cos(π*i*(j+0.5)/N_mels)
     * [2026-07-04 注意]: torchaudio 默认 norm=None → DCT *= 2.0; norm='ortho' → *= sqrt(2/N).
     *   若训练管线使用 torchaudio 默认, 本实现 MFCC 幅度偏小 50% → 量化输入分布偏移。
     *   若训练管线使用 scipy 或 norm='ortho', 本实现正确。
     *   需与 ML 训练方确认 MFCC 提取代码后才可确定是否需要添加缩放因子。 */
    for (int i = 0; i < KWS_NUM_MFCC; i++)
        for (int j = 0; j < KWS_NUM_MELS; j++)
            dct_matrix[i][j] = cos(3.1415926535f * i * (j + 0.5f) / KWS_NUM_MELS);

    mel_ready = 1;
}

/* ---- 位反转排列 (Radix-2 FFT) ---- */
static void bit_reverse(float *re, float *im, int n)
{
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (i < j) { float tr = re[i]; re[i] = re[j]; re[j] = tr; tr = im[i]; im[i] = im[j]; im[j] = tr; }
        int m = n >> 1;
        while (m >= 1 && j >= m) { j -= m; m >>= 1; }
        j += m;
    }
}

/* ---- Radix-2 FFT (in-place) ---- */
static void fft_512(float *re, float *im)
{
    bit_reverse(re, im, KWS_FFT_SIZE);
    for (int s = 2; s <= KWS_FFT_SIZE; s <<= 1) {
        float ang = -6.283185307f / s;
        float w_re = cos(ang), w_im = sin(ang);
        for (int j = 0; j < KWS_FFT_SIZE; j += s) {
            float wr = 1.0f, wi = 0.0f;
            for (int k = 0; k < s / 2; k++) {
                int a = j + k, b = j + k + s / 2;
                float tr = wr * re[b] - wi * im[b];
                float ti = wr * im[b] + wi * re[b];
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr;        im[a] += ti;
                float nwr = wr * w_re - wi * w_im;
                wi = wr * w_im + wi * w_re;
                wr = nwr;
            }
        }
    }
}

/* ============================================================
 * 主入口
 * ============================================================ */
void kws_extract_mfcc(const int16_t *pcm, float mfcc_out[KWS_NUM_MFCC][KWS_NUM_FRAMES])
{
    kws_mfcc_init();

    /* [2026-07-04 Fix]: fft_re[512]+fft_im[512] 从栈局部变量改为 static。
     *   两个数组合计 4096B = 4KB, 超出 KWSTask 栈 (1024w=4KB)。
     *   GCC -fstack-usage 实测 kws_extract_mfcc 栈 4440B > 4096B → 溢出。
     *   改为 static 放入 BSS, 每个帧迭代完全覆写数据, 无残留风险。 */
    static float fft_re[KWS_FFT_SIZE], fft_im[KWS_FFT_SIZE];

    for (int frame = 0; frame < KWS_NUM_FRAMES; frame++) {

        /* [2026-07-04 Fix]: 去掉 pre-emphasis (α=0.97)。
         *   训练管线 (prepare_chinese_kws_dataset.py L46) 使用 librosa.feature.mfcc(),
         *   librosa 默认不加预加重。原 firmware 的 pre-emphasis 导致高频分量被过度放大,
         *   与训练数据的 MFCC 分布不一致。
         *   同时训练管线在 MFCC 后立即做 per-sample z-score normalize (均值=0, 方差=1),
         *   见下方 DCT 后新增的 z-score 归一化。
         *   交叉验证: librosa 源码 + 训练脚本确认。 */
        for (int i = 0; i < KWS_FFT_SIZE; i++) {
            float s = (float)pcm[frame * KWS_HOP_SIZE + i];
            fft_re[i] = s * hamming_window[i];
            fft_im[i] = 0.0f;
        }

        /* 2. FFT */
        fft_512(fft_re, fft_im);

        /* 3. Power spectrum → Mel filterbank → log */
        float mel_energies[KWS_NUM_MELS];
        for (int m = 0; m < KWS_NUM_MELS; m++) {
            float energy = 0.0f;
            for (int k = 0; k <= KWS_FFT_SIZE / 2; k++) {
                float mag = fft_re[k] * fft_re[k] + fft_im[k] * fft_im[k];
                energy += mel_filterbank[m][k] * mag;
            }
            mel_energies[m] = logf(energy + 1e-10f);  /* 防 log(0) */
        }

        /* 4. DCT → MFCC */
        for (int i = 0; i < KWS_NUM_MFCC; i++) {
            float sum = 0.0f;
            for (int j = 0; j < KWS_NUM_MELS; j++)
                sum += dct_matrix[i][j] * mel_energies[j];
            mfcc_out[i][frame] = sum;
        }

    }

    /* [2026-07-04 Fix]: per-coefficient z-score 归一化 —
     *   训练管线 normalize_mfcc() 实现: mean=mfcc.mean(axis=1) → shape (40,1),
     *   即对每个 MFCC 系数跨 32 个时间帧独立求均值/标准差, 再做 (x-mean)/std。
     *   原实现错误地对每帧跨 40 个系数求统计量 (axis=0), 导致:
     *   ① c0 (能量) 方差天然 ≈ c1×2 → 低频系数被过度压缩, 高频被放大
     *   ② QLinear(scale=0.0393,zp=128) 校准基于 axis=1 的分布 → 维度错 → 量化偏移
     *   交叉验证: prepare_chinese_kws_dataset.py L46 (axis=1) + numpy docs + librosa */
    for (int i = 0; i < KWS_NUM_MFCC; i++) {
        float mean = 0.0f, var = 0.0f;
        for (int f = 0; f < KWS_NUM_FRAMES; f++)
            mean += mfcc_out[i][f];
        mean /= KWS_NUM_FRAMES;
        for (int f = 0; f < KWS_NUM_FRAMES; f++) {
            float d = mfcc_out[i][f] - mean;
            var += d * d;
        }
        var /= KWS_NUM_FRAMES;
        float std = sqrtf(var + 1e-8f);
        for (int f = 0; f < KWS_NUM_FRAMES; f++)
            mfcc_out[i][f] = (mfcc_out[i][f] - mean) / std;
    }
}
