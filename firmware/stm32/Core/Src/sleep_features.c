/**
 * @file sleep_features.c
 * @brief 鐫＄湢鍒嗘湡 56 缁寸壒寰佹彁鍙?鈥?瀹屾暣鐗?(鍚?HRV)
 *
 * 56 缁?= ACC(19) + TEMP(5) + BVP(6) + HR(6) + PPG_Rate(1) + Circadian(3) + HRV(16)
 * 5 epoch window 鈫?280 缁?鈫?sleep_model_predict()
 *
 * 浜ゅ弶楠岃瘉: 涓?Python verify_production_model.py 閫愬瓧娈靛搴? *   CMSIS-DSP: arm_rfft_fast_f32 鐢ㄤ簬 HRV 棰戝煙 (HF 0.15-0.4Hz, TP)
 *   IBI 鎻愬彇: 绉诲姩骞冲潎骞虫粦 + 涓綅鏁伴槇鍊? 涓?hr_detector.c 鍙傛暟涓€鑷? *   鏍堝畨鍏? 鎵€鏈夊ぇ鏁扮粍鍦?BSS (~90KB), FreeRTOS 浠诲姟鏍堜粎闇€ ~2KB
 */

#include "sleep_features.h"
#include "rtc_time.h"
#include <math.h>
/* [2026-07-05]: CMSIS-DSP 鏈撼鍏ラ」鐩? 浣跨敤鑷寘鍚?radix-2 FFT 鏇夸唬 arm_rfft_fast_f32 */

/* ---- 浣嶅弽杞帓鍒?(Radix-2 FFT) ---- */
static void bit_reverse_256(float *re, float *im)
{
    int j = 0;
    for (int i = 0; i < 256; i++) {
        if (i < j) { float tr = re[i]; re[i] = re[j]; re[j] = tr; tr = im[i]; im[i] = im[j]; im[j] = tr; }
        int m = 128;
        while (m >= 1 && j >= m) { j -= m; m >>= 1; }
        j += m;
    }
}

/* ---- 256-point complex FFT (in-place, radix-2 DIT) ---- */
static void fft_256(float *re, float *im)
{
    bit_reverse_256(re, im);
    for (int s = 2; s <= 256; s <<= 1) {
        float ang = -6.283185307f / s;
        float w_re = cos(ang), w_im = sin(ang);
        for (int j = 0; j < 256; j += s) {
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
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================
 * Ring buffer config
 * ============================================================ */
#define ACC_FS      104
#define PPG_FS      100
#define HR_FS       1
#define TEMP_FS     4
#define ACC_SAMPLES (SF_EPOCH_S * ACC_FS)    /* 3120 */
#define PPG_SAMPLES (SF_EPOCH_S * PPG_FS)    /* 3000 */
#define HR_SAMPLES  (SF_EPOCH_S * HR_FS)     /* 30   */
#define TEMP_SAMPLES (SF_EPOCH_S * TEMP_FS)  /* 120  */

/* Ring buffers: 5 epochs 鈥?all in BSS, not stack */
static int16_t  acc_buf[SF_N_WINDOWS][ACC_SAMPLES * 3];
static uint32_t ppg_buf[SF_N_WINDOWS][PPG_SAMPLES];
static float    hr_buf [SF_N_WINDOWS][HR_SAMPLES];
static float    tmp_buf[SF_N_WINDOWS][TEMP_SAMPLES];
static int      epoch_idx = 0;
static int      epoch_cnt[SF_N_WINDOWS];
static int      epoch_full = 0;
static uint8_t  epoch_valid[SF_N_WINDOWS];
static uint8_t  features_pending;
_Static_assert(SF_EPOCH_S * ACC_FS % 26 == 0, "ACC epoch must contain whole batches");

/* Work buffers 鈥?reused across feature computations to stay in BSS */
static float w_acc  [ACC_SAMPLES];      /* 3120脳4  */
static float w_ax   [ACC_SAMPLES];
static float w_ay   [ACC_SAMPLES];
static float w_az   [ACC_SAMPLES];
static float w_ibi  [200];
static float w_smth [PPG_SAMPLES];
static float w_sort [PPG_SAMPLES];
static float w_4hz  [256];
static float w_fout [256];
static float fft_im [256];  /* [2026-07-05]: 鑷寘鍚?FFT 铏氶儴, 鍏嶉櫎 CMSIS-DSP */

/* ============================================================
 * Statistics helpers (destructive 鈥?sort input in place)
 * ============================================================ */
static int cmp_f(const void *a, const void *b) {
    float d = *(const float*)a - *(const float*)b;
    return (d > 0) - (d < 0);
}

static float mean(const float *x, int n) {
    float s = 0; for (int i = 0; i < n; i++) s += x[i]; return s / n;
}
static float stdev(const float *x, int n, float m) {
    float s = 0; for (int i = 0; i < n; i++) { float d = x[i] - m; s += d * d; }
    return sqrtf(s / n);
}
static float maximum(const float *x, int n) {
    float m = x[0]; for (int i = 1; i < n; i++) if (x[i] > m) m = x[i]; return m;
}
static float minimum(const float *x, int n) {
    float m = x[0]; for (int i = 1; i < n; i++) if (x[i] < m) m = x[i]; return m;
}
static float median_inplace(float *x, int n) {
    qsort(x, n, sizeof(float), cmp_f);
    return n % 2 ? x[n/2] : (x[n/2-1] + x[n/2]) * 0.5f;
}
static float trimmed_mean(float *x, int n, float trim) {
    int k = (int)(n * trim);
    if (k < 1 || n - 2*k < 1) return mean(x, n);
    qsort(x, n, sizeof(float), cmp_f);
    float s = 0; for (int i = k; i < n - k; i++) s += x[i];
    return s / (n - 2*k);
}
static float trimmed_max(float *x, int n, float trim) {
    int k = (int)(n * trim); if (k < 1 || n-2*k < 1) k = 0;
    qsort(x, n, sizeof(float), cmp_f);
    return x[n - 1 - k];
}
static float iqr_inplace(float *x, int n, float trim) {
    int k = (int)(n * trim); if (k < 1 || n-2*k < 1) k = 0;
    qsort(x, n, sizeof(float), cmp_f);
    int q1 = (n - 2*k) / 4 + k, q3 = 3*(n - 2*k)/4 + k;
    return x[q3] - x[q1];
}
static float range(const float *x, int n) {
    return maximum(x, n) - minimum(x, n);
}

/* ============================================================
 * ACC 19 features for one epoch (3 axes 脳 6 stats + ACC_INDEX)
 * ============================================================ */
static void acc_19(const int16_t *acc, float *f) {
    /* Deinterleave */
    for (int i = 0; i < ACC_SAMPLES; i++) {
        float ax = (float)acc[i*3+0], ay = (float)acc[i*3+1], az = (float)acc[i*3+2];
        w_ax[i] = ax; w_ay[i] = ay; w_az[i] = az;
    }
    for (int axis = 0; axis < 3; axis++) {
        float *d = axis == 0 ? w_ax : (axis == 1 ? w_ay : w_az);
        memcpy(w_acc, d, ACC_SAMPLES * sizeof(float));
        float trim = 0.05f;
        *f++ = trimmed_mean(w_acc, ACC_SAMPLES, trim);
        memcpy(w_acc, d, ACC_SAMPLES * sizeof(float));
        *f++ = trimmed_max(w_acc, ACC_SAMPLES, trim);
        memcpy(w_acc, d, ACC_SAMPLES * sizeof(float));
        *f++ = iqr_inplace(w_acc, ACC_SAMPLES, trim);
        /* MAD (Median Absolute Deviation) */
        memcpy(w_acc, d, ACC_SAMPLES * sizeof(float));
        float med = median_inplace(w_acc, ACC_SAMPLES);
        for (int i = 0; i < ACC_SAMPLES; i++) w_acc[i] = fabsf(d[i] - med);
        *f++ = trimmed_mean(w_acc, ACC_SAMPLES, trim);
        memcpy(w_acc, d, ACC_SAMPLES * sizeof(float));
        for (int i = 0; i < ACC_SAMPLES; i++) w_acc[i] = fabsf(d[i] - med);
        *f++ = trimmed_max(w_acc, ACC_SAMPLES, trim);
        memcpy(w_acc, d, ACC_SAMPLES * sizeof(float));
        for (int i = 0; i < ACC_SAMPLES; i++) w_acc[i] = fabsf(d[i] - med);
        *f++ = iqr_inplace(w_acc, ACC_SAMPLES, trim);
    }
    /* ACC_INDEX: magnitude std */
    float ms = 0, mq = 0;
    for (int i = 0; i < ACC_SAMPLES; i++) {
        float m = sqrtf(w_ax[i]*w_ax[i] + w_ay[i]*w_ay[i] + w_az[i]*w_az[i]);
        ms += m; mq += m * m;
    }
    *f++ = sqrtf(mq/ACC_SAMPLES - (ms/ACC_SAMPLES)*(ms/ACC_SAMPLES));
}

/* ============================================================
 * IBI extraction: PPG IR 鈫?Inter-Beat Intervals (ms)
 * ============================================================ */
static int extract_ibi(const uint32_t *ppg, int n, float *out, int max_out) {
    if (n < PPG_FS * 2) return 0;
    /* Moving-average smooth (window 7 鈮?Savitzky-Golay substitute) */
    float sum = 0; int wnd = 7;
    for (int i = 0; i < n; i++) {
        sum += (float)ppg[i];
        if (i >= wnd) sum -= (float)ppg[i - wnd];
        w_smth[i] = (i >= wnd-1) ? sum / wnd : (float)ppg[i];
    }
    /* Median threshold */
    memcpy(w_sort, w_smth, n * sizeof(float));
    float thr = median_inplace(w_sort, n);
    /* Peak detection: distance 鈮?fs*0.3 (30 samples), height 鈮?threshold */
    int peaks[200], np = 0;
    int min_dist = (int)(PPG_FS * 0.3f);
    for (int i = min_dist; i < n - min_dist && np < 200; i++) {
        if (w_smth[i] > thr &&
            w_smth[i] > w_smth[i-1] && w_smth[i] > w_smth[i+1] &&
            w_smth[i] >= w_smth[i-min_dist/2] && w_smth[i] >= w_smth[i+min_dist/2]) {
            peaks[np++] = i;
            i += min_dist - 1;
        }
    }
    if (np < 2) return 0;
    int cnt = 0;
    for (int i = 1; i < np && cnt < max_out; i++)
        out[cnt++] = (peaks[i] - peaks[i-1]) * 1000.0f / PPG_FS;  /* ms */
    return cnt;
}

/* ============================================================
 * Single-epoch 56-feature computation
 * ============================================================ */
static void compute_epoch(const int16_t *acc, const uint32_t *ppg,
                          const float *hr, const float *tmp, int hour,
                          float f[SF_N_FEATURES])
{
    float *p = f;
    acc_19(acc, p); p += 19;

    /* TEMP 5 */
    *p++ = mean(tmp, TEMP_SAMPLES);
    memcpy(w_acc, tmp, TEMP_SAMPLES * sizeof(float)); *p++ = median_inplace(w_acc, TEMP_SAMPLES);
    *p++ = maximum(tmp, TEMP_SAMPLES); *p++ = minimum(tmp, TEMP_SAMPLES);
    *p++ = stdev(tmp, TEMP_SAMPLES, mean(tmp, TEMP_SAMPLES));

    /* BVP (PPG) 6 */
    float b_mean = 0; for (int i = 0; i < PPG_SAMPLES; i++) b_mean += (float)ppg[i]; b_mean /= PPG_SAMPLES; *p++ = b_mean;
    { for (int i = 0; i < PPG_SAMPLES; i++) w_acc[i] = (float)ppg[i]; *p++ = median_inplace(w_acc, PPG_SAMPLES); }
    { float mx=(float)ppg[0],mn=(float)ppg[0]; for(int i=1;i<PPG_SAMPLES;i++){float v=(float)ppg[i];if(v>mx)mx=v;if(v<mn)mn=v;} *p++=mx; *p++=mn; *p++=mx-mn; }
    float bv_std = 0; for (int i = 0; i < PPG_SAMPLES; i++) { float d = (float)ppg[i] - b_mean; bv_std += d*d; } *p++ = sqrtf(bv_std/PPG_SAMPLES);

    /* HR 6 */
    *p++ = mean(hr, HR_SAMPLES);
    memcpy(w_acc, hr, HR_SAMPLES * sizeof(float)); *p++ = median_inplace(w_acc, HR_SAMPLES);
    *p++ = maximum(hr, HR_SAMPLES); *p++ = minimum(hr, HR_SAMPLES);
    *p++ = range(hr, HR_SAMPLES);
    float hr_m = mean(hr, HR_SAMPLES); *p++ = stdev(hr, HR_SAMPLES, hr_m);

    /* PPG_Rate_Mean */
    *p++ = mean(hr, HR_SAMPLES);

    /* Circadian 3 */
    float hf = (float)(hour % 24) + 0.5f;
    *p++ = cos(2.0f * 3.14159265f * hf / 24.0f);
    *p++ = expf(-hf / 8.0f);
    *p++ = hf / 24.0f;

    /* HRV 16 (14 time + 2 freq) */
    int n_ibi = extract_ibi(ppg, PPG_SAMPLES, w_ibi, 200);
    if (n_ibi >= 5) {
        float mi = mean(w_ibi, n_ibi); *p++ = mi;                              /* MeanNN */
        float sd = stdev(w_ibi, n_ibi, mi); *p++ = sd;                         /* SDNN */
        float rs = 0; for (int i=1;i<n_ibi;i++) { float d=w_ibi[i]-w_ibi[i-1]; rs+=d*d; } *p++ = sqrtf(rs/(n_ibi-1)); /* RMSSD */
        float md_i = 0; for (int i=1;i<n_ibi;i++) md_i+=w_ibi[i]-w_ibi[i-1]; md_i/=(n_ibi-1);
        float ss = 0; for (int i=1;i<n_ibi;i++) { float d=(w_ibi[i]-w_ibi[i-1])-md_i; ss+=d*d; } *p++ = sqrtf(ss/(n_ibi-2)); /* SDSD */
        memcpy(w_acc, w_ibi, n_ibi*sizeof(float)); *p++ = median_inplace(w_acc, n_ibi); /* MedianNN */
        *p++ = minimum(w_ibi, n_ibi); *p++ = maximum(w_ibi, n_ibi);           /* MinNN, MaxNN */
        int n50=0,n20=0; for(int i=1;i<n_ibi;i++){float d=w_ibi[i]-w_ibi[i-1];if(d>50)n50++;if(d>20)n20++;} *p++=(float)n50/(n_ibi-1); *p++=(float)n20/(n_ibi-1);
        *p++ = sd / mi;                                                       /* CVNN */
        float cv_s=0; for(int i=1;i<n_ibi;i++){float d=w_ibi[i]-w_ibi[i-1];cv_s+=d*d;} cv_s=sqrtf(cv_s/(n_ibi-1)); *p++=cv_s/mi; /* CVSD */
        memcpy(w_acc, w_ibi, n_ibi*sizeof(float)); *p++ = iqr_inplace(w_acc, n_ibi, 0); /* IQRNN */
        *p++ = maximum(w_ibi,n_ibi) - minimum(w_ibi,n_ibi);                  /* HTI */
        *p++ = (maximum(w_ibi,n_ibi) > minimum(w_ibi,n_ibi)) ? n_ibi/(maximum(w_ibi,n_ibi)-minimum(w_ibi,n_ibi))*1000.0f : 0; /* TINN */

        /* Freq: IBI 鈫?4Hz resample 鈫?Hann 鈫?FFT 鈫?HF + TP */
        float cum_t = 0; for (int i = 1; i < n_ibi; i++) cum_t += w_ibi[i] / 1000.0f;
        if (cum_t < 1.0f) { *p++ = 0; *p++ = 0; }
        else {
            #define FN 256
            memset(w_4hz, 0, FN * sizeof(float));
            float dt = cum_t / (FN - 1);
            { /* Nearest-neighbor resample (鈮坙inear interp, good enough for PSD) */
                float c = 0; int j = 1;
                for (int i = 0; i < FN; i++) {
                    float t = i * dt;
                    while (j < n_ibi - 1 && c < t) c += w_ibi[j++] / 1000.0f;
                    w_4hz[i] = (j < n_ibi) ? w_ibi[j] : w_ibi[n_ibi-1];
                }
            }
            for (int i = 0; i < FN; i++) w_4hz[i] *= 0.5f * (1.0f - cos(2.0f * 3.14159265f * i / (FN - 1)));
            /* [2026-07-05]: 鑷寘鍚?radix-2 FFT 鏇夸唬 CMSIS-DSP arm_rfft_fast_f32.
             *   杈撳叆: w_4hz[256] 瀹炰俊鍙?(Hann 绐楀凡涔?.
             *   杈撳嚭: w_fout[256] = [DC, Nyquist, re1, im1, re2, im2, ...] */
            memset(fft_im, 0, sizeof(fft_im));
            memcpy(w_fout, w_4hz, FN * sizeof(float));
            fft_256(w_fout, fft_im);
            /* 鎵撳寘涓?CMSIS 鏍煎紡: 浠庨珮浣嶅悜浣庝綅 shift, 閬垮厤瑕嗙洊鏈鏁版嵁 */
            /* [2026-08-27 Fix]: save Nyquist before i=64 overwrites w_fout[128] */
            float hrv_nyquist = w_fout[FN/2];
            for (int i = FN/2 - 1; i >= 1; i--) {
                w_fout[2*i]     = w_fout[i];
                w_fout[2*i + 1] = fft_im[i];
            }
            w_fout[1] = hrv_nyquist;  /* Nyquist 鈫?position 1 */
            /* w_fout[0] = DC (淇濇寔鍘熶綅) */
            float tp = w_fout[0]*w_fout[0] + w_fout[1]*w_fout[1]; /* DC + Nyquist */
            float hf_pow = 0;
            for (int i = 1; i < FN / 2; i++) {
                float re = w_fout[2*i], im = w_fout[2*i+1];
                float pwr = re * re + im * im;
                float freq = i / cum_t;  /* [2026-07-06 Fix]: 鍘?i*4.0f/FN 纭紪鐮?4Hz (鍋囪 cum_t=64s)銆?                                           瀹為檯閲囨牱鐜?256/cum_t, bin spacing=1/cum_t銆傞潤鎭疕R 75BPM鏃?cum_t鈮?2s,
                                           HF棰戞鍋忕Щ~23%銆備氦鍙夐獙璇? FFT 棰戠巼鍒嗚鲸鐜囧叕寮?螖f=Fs/N=1/T_total銆?*/
                tp += pwr;
                if (freq >= 0.15f && freq <= 0.4f) hf_pow += pwr;
            }
            *p++ = hf_pow; *p++ = tp;
        }
    } else {
        for (int i = 0; i < 16; i++) *p++ = 0.0f;
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

void sleep_features_feed(const int16_t *acc26, const uint32_t *ppg_ir,
                         uint8_t ppg_count, float hr, float temp_c)
{
    int e = epoch_idx;
    int n = epoch_cnt[e];
    int batch = n / 26;
    features_pending = 0; /* A new batch starts overwriting the oldest epoch. */
    if (n == 0) epoch_valid[e] = 1;
    memcpy(&acc_buf[e][n * 3], acc26, 26 * 3 * sizeof(int16_t));
    /* Keep the PPG timeline even on a short read, but never infer on that window. */
    int count = ppg_count <= 25 ? ppg_count : 25;
    memset(&ppg_buf[e][batch * 25], 0, 25 * sizeof(uint32_t));
    if (count > 0) memcpy(&ppg_buf[e][batch * 25], ppg_ir, count * sizeof(uint32_t));
    if (ppg_count != 25) epoch_valid[e] = 0;
    hr_buf[e][batch / 4] = hr;
    tmp_buf[e][batch] = temp_c;
    epoch_cnt[e] += 26;
    if (epoch_cnt[e] == ACC_SAMPLES) {
        epoch_idx = (e + 1) % SF_N_WINDOWS;
        epoch_cnt[epoch_idx] = 0;
        if (e + 1 == SF_N_WINDOWS) epoch_full = 1;
        features_pending = epoch_full;
        for (int i = 0; i < SF_N_WINDOWS; i++)
            if (!epoch_valid[i]) features_pending = 0;
    }
}

int sleep_features_ready(void) {
    return features_pending;
}

void sleep_features_get(float *features) {
    features_pending = 0;
    float ef[SF_N_WINDOWS][SF_N_FEATURES];
    for (int w = 0; w < SF_N_WINDOWS; w++) {
        int src = (epoch_idx + w) % SF_N_WINDOWS;
        uint8_t h, m, s; rtc_get_hms(&h, &m, &s);
        compute_epoch(acc_buf[src], ppg_buf[src], hr_buf[src], tmp_buf[src], (int)h, ef[w]);
    }
    for (int i = 0; i < SF_N_WINDOWS * SF_N_FEATURES; i++)
        features[i] = ((float*)ef)[i];
}
