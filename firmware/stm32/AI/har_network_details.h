/**
  ******************************************************************************
  * @file    network.h
  * @date    2026-07-12T00:50:47+0800
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

const stai_network_details g_har_network_details = {
  .tensors = (const stai_tensor[27]) {
   { .size_bytes = 3072, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 6, 512}}, .scale = {1, (const float[1]){1.0274745225906372}}, .zeropoint = {1, (const int16_t[1]){104}}, .name = "imu_output" },
   { .size_bytes = 3072, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 512, 6}}, .scale = {1, (const float[1]){1.0274745225906372}}, .zeropoint = {1, (const int16_t[1]){104}}, .name = "imu_Transpose_output" },
   { .size_bytes = 16384, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 512, 32}}, .scale = {1, (const float[1]){0.11783938109874725}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_block1_block1_2_Relu_output_0_output" },
   { .size_bytes = 8192, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 256, 32}}, .scale = {1, (const float[1]){0.08757071197032928}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_block1_block1_5_Relu_output_0_output" },
   { .size_bytes = 32, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 1, 32}}, .scale = {1, (const float[1]){0.015352572314441204}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se1_pool_GlobalAveragePool_output_0_output" },
   { .size_bytes = 8, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 8}}, .scale = {1, (const float[1]){0.013737290166318417}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se1_fc_fc_1_Relu_output_0_output" },
   { .size_bytes = 32, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 32}}, .scale = {1, (const float[1]){0.020577842369675636}}, .zeropoint = {1, (const int16_t[1]){128}}, .name = "_se1_fc_fc_2_MatMul_output_0_output" },
   { .size_bytes = 32, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 32}}, .scale = {1, (const float[1]){0.003651697188615799}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se1_fc_fc_3_Sigmoid_output_0_output" },
   { .size_bytes = 8192, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 256, 32}}, .scale = {1, (const float[1]){0.037766631692647934}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se1_Mul_output_0_output" },
   { .size_bytes = 16384, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 256, 64}}, .scale = {1, (const float[1]){0.07459980249404907}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_block2_block2_2_Relu_output_0_output" },
   { .size_bytes = 8192, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 128, 64}}, .scale = {1, (const float[1]){0.06596948206424713}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_block2_block2_5_Relu_output_0_output" },
   { .size_bytes = 64, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 1, 64}}, .scale = {1, (const float[1]){0.015314740128815174}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se2_pool_GlobalAveragePool_output_0_output" },
   { .size_bytes = 16, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 16}}, .scale = {1, (const float[1]){0.020675819367170334}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se2_fc_fc_1_Relu_output_0_output" },
   { .size_bytes = 64, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 64}}, .scale = {1, (const float[1]){0.027712274342775345}}, .zeropoint = {1, (const int16_t[1]){160}}, .name = "_se2_fc_fc_2_MatMul_output_0_output" },
   { .size_bytes = 64, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 64}}, .scale = {1, (const float[1]){0.0036551551893353462}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se2_fc_fc_3_Sigmoid_output_0_output" },
   { .size_bytes = 8192, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 128, 64}}, .scale = {1, (const float[1]){0.025273581966757774}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se2_Mul_output_0_output" },
   { .size_bytes = 16384, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 128, 128}}, .scale = {1, (const float[1]){0.06196974590420723}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_block3_block3_2_Relu_output_0_output" },
   { .size_bytes = 128, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 1, 128}}, .scale = {1, (const float[1]){0.014108291827142239}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se3_pool_GlobalAveragePool_output_0_output" },
   { .size_bytes = 32, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 32}}, .scale = {1, (const float[1]){0.02771262638270855}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se3_fc_fc_1_Relu_output_0_output" },
   { .size_bytes = 128, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 128}}, .scale = {1, (const float[1]){0.052780766040086746}}, .zeropoint = {1, (const int16_t[1]){16}}, .name = "_se3_fc_fc_2_MatMul_output_0_output" },
   { .size_bytes = 128, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 128}}, .scale = {1, (const float[1]){0.003921555355191231}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se3_fc_fc_3_Sigmoid_output_0_output" },
   { .size_bytes = 16384, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 128, 128}}, .scale = {1, (const float[1]){0.06168152764439583}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_se3_Mul_output_0_output" },
   { .size_bytes = 128, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {3, (const int32_t[3]){1, 1, 128}}, .scale = {1, (const float[1]){0.014091073535382748}}, .zeropoint = {1, (const int16_t[1]){0}}, .name = "_gap_GlobalAveragePool_output_0_output" },
   { .size_bytes = 512, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 1, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_gap_GlobalAveragePool_output_0_0_0__classifier_classifier_2_Relu_output_0_conversion_output" },
   { .size_bytes = 256, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {2, (const int32_t[2]){1, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_classifier_classifier_2_Relu_output_0_output" },
   { .size_bytes = 28, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {2, (const int32_t[2]){1, 7}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "logits_QuantizeLinear_Input_output" },
   { .size_bytes = 7, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_U8, .shape = {2, (const int32_t[2]){1, 7}}, .scale = {1, (const float[1]){0.09788865596055984}}, .zeropoint = {1, (const int16_t[1]){113}}, .name = "logits_QuantizeLinear_Input_0_conversion_output" }
  },
  .nodes = (const stai_node_details[26]){
    {.id = 2, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){0}}, .output_tensors = {1, (const int32_t[1]){1}} }, /* imu_Transpose */
    {.id = 44, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){1}}, .output_tensors = {1, (const int32_t[1]){2}} }, /* _block1_block1_2_Relu_output_0 */
    {.id = 50, .type = AI_LAYER_OPTIMIZED_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){2}}, .output_tensors = {1, (const int32_t[1]){3}} }, /* _block1_block1_5_Relu_output_0 */
    {.id = 55, .type = AI_LAYER_POOL_TYPE, .input_tensors = {1, (const int32_t[1]){3}}, .output_tensors = {1, (const int32_t[1]){4}} }, /* _se1_pool_GlobalAveragePool_output_0 */
    {.id = 69, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){4}}, .output_tensors = {1, (const int32_t[1]){5}} }, /* _se1_fc_fc_1_Relu_output_0 */
    {.id = 72, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){5}}, .output_tensors = {1, (const int32_t[1]){6}} }, /* _se1_fc_fc_2_MatMul_output_0 */
    {.id = 75, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){6}}, .output_tensors = {1, (const int32_t[1]){7}} }, /* _se1_fc_fc_3_Sigmoid_output_0 */
    {.id = 81, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){3, 7}}, .output_tensors = {1, (const int32_t[1]){8}} }, /* _se1_Mul_output_0 */
    {.id = 84, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){8}}, .output_tensors = {1, (const int32_t[1]){9}} }, /* _block2_block2_2_Relu_output_0 */
    {.id = 90, .type = AI_LAYER_OPTIMIZED_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){9}}, .output_tensors = {1, (const int32_t[1]){10}} }, /* _block2_block2_5_Relu_output_0 */
    {.id = 95, .type = AI_LAYER_POOL_TYPE, .input_tensors = {1, (const int32_t[1]){10}}, .output_tensors = {1, (const int32_t[1]){11}} }, /* _se2_pool_GlobalAveragePool_output_0 */
    {.id = 109, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){11}}, .output_tensors = {1, (const int32_t[1]){12}} }, /* _se2_fc_fc_1_Relu_output_0 */
    {.id = 112, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){12}}, .output_tensors = {1, (const int32_t[1]){13}} }, /* _se2_fc_fc_2_MatMul_output_0 */
    {.id = 115, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){13}}, .output_tensors = {1, (const int32_t[1]){14}} }, /* _se2_fc_fc_3_Sigmoid_output_0 */
    {.id = 121, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){10, 14}}, .output_tensors = {1, (const int32_t[1]){15}} }, /* _se2_Mul_output_0 */
    {.id = 124, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){15}}, .output_tensors = {1, (const int32_t[1]){16}} }, /* _block3_block3_2_Relu_output_0 */
    {.id = 129, .type = AI_LAYER_POOL_TYPE, .input_tensors = {1, (const int32_t[1]){16}}, .output_tensors = {1, (const int32_t[1]){17}} }, /* _se3_pool_GlobalAveragePool_output_0 */
    {.id = 143, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){17}}, .output_tensors = {1, (const int32_t[1]){18}} }, /* _se3_fc_fc_1_Relu_output_0 */
    {.id = 146, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){18}}, .output_tensors = {1, (const int32_t[1]){19}} }, /* _se3_fc_fc_2_MatMul_output_0 */
    {.id = 149, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){19}}, .output_tensors = {1, (const int32_t[1]){20}} }, /* _se3_fc_fc_3_Sigmoid_output_0 */
    {.id = 155, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){16, 20}}, .output_tensors = {1, (const int32_t[1]){21}} }, /* _se3_Mul_output_0 */
    {.id = 158, .type = AI_LAYER_POOL_TYPE, .input_tensors = {1, (const int32_t[1]){21}}, .output_tensors = {1, (const int32_t[1]){22}} }, /* _gap_GlobalAveragePool_output_0 */
    {.id = 158, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){22}}, .output_tensors = {1, (const int32_t[1]){23}} }, /* _gap_GlobalAveragePool_output_0_0_0__classifier_classifier_2_Relu_output_0_conversion */
    {.id = 164, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){23}}, .output_tensors = {1, (const int32_t[1]){24}} }, /* _classifier_classifier_2_Relu_output_0 */
    {.id = 167, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){24}}, .output_tensors = {1, (const int32_t[1]){25}} }, /* logits_QuantizeLinear_Input */
    {.id = 167, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){25}}, .output_tensors = {1, (const int32_t[1]){26}} } /* logits_QuantizeLinear_Input_0_conversion */
  },
  .n_nodes = 26
};
#endif

