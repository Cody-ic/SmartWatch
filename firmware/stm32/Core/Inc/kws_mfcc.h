/**
 * @file    kws_mfcc.h
 * @brief   KWS MFCC 特征提取 — 自包含实现, 不依赖 CMSIS-DSP
 *
 * [2026-07-04]: KWS 集成。使用 FPU 直接计算, STM32U5 M33 FPU 单周期 float 乘加。
 *   输入:  16kHz PCM (实际~13.5kHz, 模型噪声增强可容忍频偏)
 *   输出:  (1, 40, 32) float32 — 40 MFCC × 32 帧
 *   参考:  python_speech_features / torchaudio MFCC 实现
 */

#ifndef KWS_MFCC_H
#define KWS_MFCC_H

#include <stdint.h>

#define KWS_SAMPLE_RATE     16000   /* 目标采样率 (实际约 13.5kHz) */
#define KWS_FFT_SIZE        512     /* FFT 点数 */
#define KWS_HOP_SIZE        512     /* 跳步 (无重叠) */
#define KWS_NUM_MELS        64      /* Mel 滤波器数 */
#define KWS_NUM_MFCC        40      /* MFCC 系数数 (取前 40) */
#define KWS_NUM_FRAMES      32      /* 帧数 */
#define KWS_AUDIO_SAMPLES   16384   /* 512 × 32 = 16384 样本 ≈ 1.024s @ 16kHz */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 从 PCM 音频数据提取 MFCC 特征
 * @param pcm      PCM 样本 (int16, 16384 个)
 * @param mfcc_out 输出: float[40][32] 按帧存储 (mfcc_out[coeff][frame])
 */
void kws_extract_mfcc(const int16_t *pcm, float mfcc_out[KWS_NUM_MFCC][KWS_NUM_FRAMES]);

#ifdef __cplusplus
}
#endif

#endif /* KWS_MFCC_H */
