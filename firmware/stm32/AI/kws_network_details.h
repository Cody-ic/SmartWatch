/**
  ******************************************************************************
  * @file    network.h
  * @date    2026-07-12T00:51:40+0800
  * @brief   ST.AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */
#ifndef STAI_NETWORK_DETAILS_H
#define STAI_NETWORK_DETAILS_H

#include "stai.h"
#include "layers.h"

const stai_network_details g_network_details = {
  .tensors = (const stai_tensor[11]) {
   { .size_bytes = 1280, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {4, (const int32_t[4]){1, 40, 32, 1}}, .scale = {1, (const float[1]){0.03931417316198349}}, .zeropoint = {1, (const int16_t[1]){128}}, .name = "mfcc_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {4, (const int32_t[4]){1, 16, 15, 64}}, .scale = {1, (const float[1]){0.02343541383743286}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "relu_output" },
   { .size_bytes = 4096, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {4, (const int32_t[4]){1, 8, 8, 64}}, .scale = {1, (const float[1]){0.03606655076146126}}, .zeropoint = {1, (const int16_t[1]){105}}, .name = "conv2d_1_output" },
   { .size_bytes = 4096, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {4, (const int32_t[4]){1, 8, 8, 64}}, .scale = {1, (const float[1]){0.020348364487290382}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "relu_1_output" },
   { .size_bytes = 1024, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {4, (const int32_t[4]){1, 4, 4, 64}}, .scale = {1, (const float[1]){0.020863397046923637}}, .zeropoint = {1, (const int16_t[1]){125}}, .name = "conv2d_3_output" },
   { .size_bytes = 1024, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {4, (const int32_t[4]){1, 4, 4, 64}}, .scale = {1, (const float[1]){0.027722682803869247}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "relu_2_output" },
   { .size_bytes = 4096, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 4, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "relu_2_0_0_mean_conversion_output" },
   { .size_bytes = 256, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 1, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "mean_output" },
   { .size_bytes = 256, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 1, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "mean_Mul_output" },
   { .size_bytes = 16, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {2, (const int32_t[2]){1, 4}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "logits_QuantizeLinear_Input_output" },
   { .size_bytes = 4, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 4}}, .scale = {1, (const float[1]){0.04167867824435234}}, .zeropoint = {1, (const int16_t[1]){106}}, .name = "logits_QuantizeLinear_Input_0_conversion_output" }
  },
  .nodes = (const stai_node_details[10]){
    {.id = 13, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){0}}, .output_tensors = {1, (const int32_t[1]){1}} }, /* relu */
    {.id = 16, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){1}}, .output_tensors = {1, (const int32_t[1]){2}} }, /* conv2d_1 */
    {.id = 19, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){2}}, .output_tensors = {1, (const int32_t[1]){3}} }, /* relu_1 */
    {.id = 22, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){3}}, .output_tensors = {1, (const int32_t[1]){4}} }, /* conv2d_3 */
    {.id = 25, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){4}}, .output_tensors = {1, (const int32_t[1]){5}} }, /* relu_2 */
    {.id = 25, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){5}}, .output_tensors = {1, (const int32_t[1]){6}} }, /* relu_2_0_0_mean_conversion */
    {.id = 28, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){6}}, .output_tensors = {1, (const int32_t[1]){7}} }, /* mean */
    {.id = 28, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){7}}, .output_tensors = {1, (const int32_t[1]){8}} }, /* mean_Mul */
    {.id = 32, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){8}}, .output_tensors = {1, (const int32_t[1]){9}} }, /* logits_QuantizeLinear_Input */
    {.id = 32, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){9}}, .output_tensors = {1, (const int32_t[1]){10}} } /* logits_QuantizeLinear_Input_0_conversion */
  },
  .n_nodes = 10
};
#endif

