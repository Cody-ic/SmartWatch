/**
  ******************************************************************************
  * @file    har7class_v6_int8_data_params.h
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-05-27T19:40:42+0800
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#ifndef HAR7CLASS_V6_INT8_DATA_PARAMS_H
#define HAR7CLASS_V6_INT8_DATA_PARAMS_H

#include "ai_platform.h"

/*
#define AI_HAR7CLASS_V6_INT8_DATA_WEIGHTS_PARAMS \
  (AI_HANDLE_PTR(&ai_har7class_v6_int8_data_weights_params[1]))
*/

#define AI_HAR7CLASS_V6_INT8_DATA_CONFIG               (NULL)


#define AI_HAR7CLASS_V6_INT8_DATA_ACTIVATIONS_SIZES \
  { 66048, }
#define AI_HAR7CLASS_V6_INT8_DATA_ACTIVATIONS_SIZE     (66048)
#define AI_HAR7CLASS_V6_INT8_DATA_ACTIVATIONS_COUNT    (1)
#define AI_HAR7CLASS_V6_INT8_DATA_ACTIVATION_1_SIZE    (66048)



#define AI_HAR7CLASS_V6_INT8_DATA_WEIGHTS_SIZES \
  { 86236, }
#define AI_HAR7CLASS_V6_INT8_DATA_WEIGHTS_SIZE         (86236)
#define AI_HAR7CLASS_V6_INT8_DATA_WEIGHTS_COUNT        (1)
#define AI_HAR7CLASS_V6_INT8_DATA_WEIGHT_1_SIZE        (86236)



#define AI_HAR7CLASS_V6_INT8_DATA_ACTIVATIONS_TABLE_GET() \
  (&g_har7class_v6_int8_activations_table[1])

extern ai_handle g_har7class_v6_int8_activations_table[1 + 2];



#define AI_HAR7CLASS_V6_INT8_DATA_WEIGHTS_TABLE_GET() \
  (&g_har7class_v6_int8_weights_table[1])

extern ai_handle g_har7class_v6_int8_weights_table[1 + 2];


#endif    /* HAR7CLASS_V6_INT8_DATA_PARAMS_H */
