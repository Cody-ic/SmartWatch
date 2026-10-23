/**
 * @file    har_inference.h
 * @brief   HAR inference engine - bridge between IMU samples and AI model
 */
#ifndef HAR_INFERENCE_H
#define HAR_INFERENCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui.h"    /* har_class_t */
#include <stdint.h>

/**
 * Init the HAR engine: creates AI network, resets buffers.
 * Call once after MAX30102/LSM6DSOX init.
 */
void HAR_Init(void);

/**
 * Push a batch of 25 IMU samples (already downsampled from 26).
 * Returns 1 when a full window (512 samples = ~21 calls) is ready
 * and inference has been run. UI is updated automatically.
 */
int HAR_AddSamples(int16_t *acc_25, int16_t *gyr_25);

/**
 * Get the most recent class label (0-6).
 * Useful if you want to display it elsewhere (e.g. log to UART).
 */
har_class_t HAR_GetLastClass(void);

/**
 * Get the confidence (0.0-1.0) of the last inference.
 * Returns the softmax max probability from the quantized output.
 */
float HAR_GetLastConfidence(void);

/**
 * Get human-readable class name string.
 */
const char *HAR_GetClassName(har_class_t c);

/* [2026-07-14]: Proto Network 辅助输出 */

/** @brief Proto 推理类别 (0..6), 原型未加载返回 -1 */
int HAR_GetProtoClass(void);

/** @brief Proto softmax 概率 [7], 未加载时填充 -1.0f */
void HAR_GetProtoProbs(float probs[7]);

#ifdef __cplusplus
}
#endif

#endif /* HAR_INFERENCE_H */
