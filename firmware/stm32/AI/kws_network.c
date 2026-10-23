/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-07-12T00:51:40+0800
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
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

#include "ai_lite_inspect.h"
#include "ai_platform_interface.h"
#include "layers.h"
#include "core_convert.h"
#include "kws_network.h"
#include "kws_network_details.h"
#include "kws_network_data.h"
#include "stai_events.h"

#include "ai_lite_inspect.h"

#include "lite_operators.h"
/*****************************************************************************/
#define STAI_INTERNAL_API_MAJOR               (1)
#define STAI_INTERNAL_API_MINOR               (0)
#define STAI_INTERNAL_API_MICRO               (0)

#define STAI_MAGIC                            (0xB1C00100)

/*****************************************************************************/
#define _STAI_CONCAT_ARG(a, b)     a ## b
#define STAI_CONCAT(a, b)         _STAI_CONCAT_ARG(a, b)

/*!  STAI_CAST SECTION                       *********************************/
#define STAI_CAST(type, expr) \
  ((type)(expr))


/*****************************************************************************/
#define STAI_SIZE(_size) \
  ((stai_size)(_size))

/*****************************************************************************/
#define STAI_INIT_BUFFER(_flags, _size, _address) \
  { \
    .size = (_size), \
    .address = (uintptr_t)(_address), \
    .flags = (_flags), \
  }

#define STAI_INIT_TENSOR(_name, _flags, _fmt, _size_bytes, _shape, _scale, _zeropoint) \
  { \
    .size_bytes = (_size_bytes), \
    .flags = (_flags), \
    .format = (stai_format)(_fmt), \
    .shape = STAI_PACK(_shape), \
    .scale = STAI_PACK(_scale), \
    .zeropoint = STAI_PACK(_zeropoint), \
    .name = (_name) \
  }

#define STAI_INIT_ARRAY(_size, _ptr) \
  { .size = STAI_SIZE(_size), .data = STAI_PACK(_ptr) }


#define STAI_CAST_ARRAY(_type, _size, _ptr) \
  { .size = STAI_SIZE(_size), .data = (_type)STAI_PACK(_ptr) }


#define STAI_DECLARE_ARRAY(_type, _size, ...) \
  { .size = STAI_SIZE(_size), .data = (_type[_size]) { STAI_PACK(__VA_ARGS__) } }


#define STAI_EMPTY_ARRAY() \
  { .size = 0, .data = NULL }


#define STAI_INIT_VERSION(_major, _minor, _micro) \
  { .major = (_major), .minor = (_minor), .micro = (_micro), .reserved = 0x0 }

/*****************************************************************************/
/**  Getters and setters  **/

#define STAI_GET_ARRAY_SIZE(nd_array) \
  (nd_array.size)


#define STAI_GET_ARRAY_ELEM(nd_array, pos) \
  (nd_array.data[(pos)])

#define _STAI_SET_ERROR(net_ctx, cond, value, exit) { \
  if (!(net_ctx)) { return STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE; } \
  if (((uintptr_t)net_ctx) & (_STAI_CONTEXT_ALIGNMENT-1)) { return STAI_ERROR_NETWORK_INVALID_CONTEXT_ALIGNMENT; } \
  if (((value) >= STAI_ERROR_GENERIC) && (cond)) { \
    if ((net_ctx)->_return_code == STAI_SUCCESS) { \
      (net_ctx)->_return_code = (value); \
    } \
    return (exit); \
  } \
}

/*****************************************************************************/
/* TODO REMOVE THESE TWO MACROS */
#define STAI_EVENT_NODE_START_CB
#define STAI_EVENT_NODE_STOP_CB

#ifdef STAI_EVENT_NODE_START_CB
#ifndef _STAI_NETWORK_EVENT_NODE_START_CB
  #define _STAI_NETWORK_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
  if (net_ctx->_callback) { \
    const stai_event_node_start_stop _start_event = { \
      .node_id=(_node_id), \
      .buffers={ \
        .size=(_buffers_size), \
        .data=(stai_ptr const*)(const stai_ptr[_buffers_size])STAI_PACK(__VA_ARGS__) \
      } \
    }; \
    net_ctx->_callback(net_ctx->_callback_cookie, STAI_EVENT_NODE_START, (const void*)&_start_event); \
  }
#endif
#else
  #define _STAI_NETWORK_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_NETWORK_EVENT_NODE_START_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_START_CB */

#ifdef STAI_EVENT_NODE_STOP_CB
#ifndef _STAI_NETWORK_EVENT_NODE_STOP_CB
  #define _STAI_NETWORK_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
  if (net_ctx->_callback) { \
    const stai_event_node_start_stop _stop_event = { \
      .node_id=(_node_id), \
      .buffers={ \
        .size=(_buffers_size), \
        .data=(stai_ptr const*)(stai_ptr[_buffers_size])STAI_PACK(__VA_ARGS__) \
      } \
    }; \
    net_ctx->_callback(net_ctx->_callback_cookie, STAI_EVENT_NODE_STOP, (const void*)&_stop_event); \
  }
#endif
#else
  #define _STAI_NETWORK_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_NETWORK_EVENT_NODE_STOP_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_STOP_CB */


/*****************************************************************************/
#define _STAI_NETWORK_MODEL_SIGNATURE     "0xfddbddba513c4212047b068b4a79ad5c"
#define _STAI_NETWORK_DATETIME            "2026-07-12T00:51:40+0800"
#define _STAI_NETWORK_COMPILE_DATETIME    __DATE__ " " __TIME__

#define _STAI_CONTEXT_ALIGNMENT        STAI_NETWORK_CONTEXT_ALIGNMENT

/*****************************************************************************/
#define g_network_activations_1     (NULL)




#if defined(HAVE_NETWORK_INFO)
/*****************************************************************************/
static const stai_network_info g_network_info = {
  .model_signature = _STAI_NETWORK_MODEL_SIGNATURE,
  .c_compile_datetime = _STAI_NETWORK_COMPILE_DATETIME,
  .c_model_name = STAI_NETWORK_MODEL_NAME,
  .c_model_datetime = _STAI_NETWORK_DATETIME,
  .c_model_signature = 0x0,
  .runtime_version = STAI_INIT_VERSION(12, 0, 1),
  .tool_version = STAI_INIT_VERSION(4, 0, 1),
  .api_version = STAI_INIT_VERSION(1, 0, 0),
  .n_macc = STAI_NETWORK_MACC_NUM,
  .n_nodes = STAI_NETWORK_NODES_NUM,
  .flags = STAI_NETWORK_FLAGS,
  .n_inputs = STAI_NETWORK_IN_NUM,
  .n_outputs = STAI_NETWORK_OUT_NUM,
  .n_activations = STAI_NETWORK_ACTIVATIONS_NUM,
  .n_weights = STAI_NETWORK_WEIGHTS_NUM,
  .n_states = STAI_NETWORK_STATES_NUM,
  .inputs = (stai_tensor[STAI_NETWORK_IN_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_IN_1_NAME,
      STAI_NETWORK_IN_1_FLAGS,
      STAI_NETWORK_IN_1_FORMAT,
      STAI_NETWORK_IN_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 4, 1, 1, 40, 32),
      STAI_DECLARE_ARRAY(float, 1, 0.03931417316198349f),
      STAI_DECLARE_ARRAY(int16_t, 1, 128)),
    },
    .outputs = (stai_tensor[STAI_NETWORK_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_OUT_1_NAME,
      STAI_NETWORK_OUT_1_FLAGS,
      STAI_NETWORK_OUT_1_FORMAT,
      STAI_NETWORK_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 2, 1, 4),
      STAI_DECLARE_ARRAY(float, 1, 0.04167867824435234f),
      STAI_DECLARE_ARRAY(int16_t, 1, 106)),
    },
  .activations = (stai_tensor[STAI_NETWORK_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 22144),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_NETWORK_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 14480),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },

  .states = NULL
};
#endif

#define _STAI_CONTEXT_ACQUIRE(_net_ctx, _net_handle) \
  _stai_network_context* _net_ctx = (_stai_network_context*)(_net_handle); \
  STAI_ASSERT(_net_ctx != NULL) \
  _STAI_SET_ERROR(_net_ctx, _net_ctx->_magic != STAI_MAGIC, \
                  STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE, _net_ctx->_return_code)


/*****************************************************************************/
static
void _stai_network_check(_stai_network_context* net_ctx)
{
  stai_size idx;

// Check activations status
  for (idx=0; idx<STAI_NETWORK_ACTIVATIONS_NUM; idx++) {
    if (net_ctx->_activations[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_ACTIVATIONS_NUM) ? STAI_FLAG_ACTIVATIONS : STAI_FLAG_NONE;
// Check inputs status
  for (idx=0; idx<STAI_NETWORK_IN_NUM; idx++) {
    if (net_ctx->_inputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_IN_NUM) ? STAI_FLAG_INPUTS : STAI_FLAG_NONE;

  // Check outputs status
  for (idx=0; idx<STAI_NETWORK_OUT_NUM; idx++) {
    if (net_ctx->_outputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_OUT_NUM) ? STAI_FLAG_OUTPUTS : STAI_FLAG_NONE;

// Check weights status
  for (idx=0; idx<STAI_NETWORK_WEIGHTS_NUM; idx++) {
    if (net_ctx->_weights[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_WEIGHTS_NUM) ? STAI_FLAG_WEIGHTS : STAI_FLAG_NONE;
STAI_PRINT("  [_stai_network_check] flags: 0x%08x\n", net_ctx->_flags)
}


/*****************************************************************************/
STAI_API_ENTRY
stai_return_code stai_network_init(
  stai_network* network)
{
  /* Memory where to store internal context is provided by applications as a raw byte buffer */
  _stai_network_context* net_ctx = (_stai_network_context*)(network);
  net_ctx->_return_code = STAI_SUCCESS;
  STAI_PRINT("[Entering Network Init] network(%p) context_size(%d)\n", net_ctx, (int32_t)sizeof(_stai_network_context))

  _STAI_SET_ERROR(net_ctx, STAI_NETWORK_CONTEXT_SIZE != sizeof(_stai_network_context),
                 STAI_ERROR_NETWORK_INVALID_CONTEXT_SIZE, net_ctx->_return_code)

  {
    const _stai_network_context _network_context = {
      ._magic = STAI_MAGIC,
      ._signature = STAI_NETWORK_MODEL_SIGNATURE,
      ._flags = STAI_NETWORK_FLAGS,
      ._return_code = STAI_SUCCESS,
      ._callback = NULL,
      ._callback_cookie = NULL,
      ._activations = {
      (stai_ptr)g_network_activations_1
      },
      ._weights = {
      (stai_ptr)g_network_weights_array
      },
      ._inputs = {
    NULL},
      ._outputs = {
    NULL},
    };

    // Deep copy of internal context to opaque buffer provided by app
    *net_ctx = _network_context;

    _stai_network_check(net_ctx);
  }

  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_deinit(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /*  Reset flags to initial state  */
  net_ctx->_flags = STAI_NETWORK_FLAGS;
  return net_ctx->_return_code;
}

/*****************************************************************************/



/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(mfcc_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03931417316198349f),
    AI_PACK_UINTQ_ZP(128)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02343541383743286f),
    AI_PACK_UINTQ_ZP(0)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003436623141169548f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_1_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03606655076146126f),
    AI_PACK_UINTQ_ZP(105)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_1_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003383936360478401f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_1_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020348364487290382f),
    AI_PACK_UINTQ_ZP(0)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_1_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010282007046043873f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_3_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020863397046923637f),
    AI_PACK_UINTQ_ZP(125)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_3_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0033188911620527506f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_2_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.027722682803869247f),
    AI_PACK_UINTQ_ZP(0)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_2_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013896459713578224f),
    AI_PACK_INTQ_ZP(0)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  mfcc_output_array, AI_ARRAY_FORMAT_U8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 1280, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  relu_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 15360, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  relu_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2560, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  relu_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  relu_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 5536, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_1_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 4096, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_1_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 576, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_1_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_1_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1729, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  relu_1_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 4096, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  relu_1_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4096, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  relu_1_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  relu_1_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 256, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_3_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 1024, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_3_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 576, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_3_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1729, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  relu_2_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 1024, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  relu_2_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4096, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  relu_2_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  relu_2_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 256, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  relu_2_0_0_mean_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1024, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  mean_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  mfcc_output, AI_STATIC,
  15, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 32, 40), AI_STRIDE_INIT(4, 1, 1, 1, 32),
  1, &mfcc_output_array, &mfcc_output_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  relu_bias, AI_STATIC,
  25, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &relu_bias_array, NULL)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  relu_output, AI_STATIC,
  26, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 15, 16), AI_STRIDE_INIT(4, 1, 1, 64, 960),
  1, &relu_output_array, &relu_output_array_intq)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  relu_scratch0, AI_STATIC,
  27, 0x0,
  AI_SHAPE_INIT(4, 1, 5536, 1, 1), AI_STRIDE_INIT(4, 1, 1, 5536, 5536),
  1, &relu_scratch0_array, NULL)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  relu_weights, AI_STATIC,
  28, 0x1,
  AI_SHAPE_INIT(4, 1, 4, 10, 64), AI_STRIDE_INIT(4, 1, 1, 64, 256),
  1, &relu_weights_array, &relu_weights_array_intq)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_1_bias, AI_STATIC,
  0, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &conv2d_1_bias_array, NULL)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_1_output, AI_STATIC,
  1, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 8, 8), AI_STRIDE_INIT(4, 1, 1, 64, 512),
  1, &conv2d_1_output_array, &conv2d_1_output_array_intq)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_1_scratch0, AI_STATIC,
  2, 0x0,
  AI_SHAPE_INIT(4, 1, 1729, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1729, 1729),
  1, &conv2d_1_scratch0_array, NULL)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_1_weights, AI_STATIC,
  3, 0x1,
  AI_SHAPE_INIT(4, 64, 3, 3, 1), AI_STRIDE_INIT(4, 1, 64, 64, 192),
  1, &conv2d_1_weights_array, &conv2d_1_weights_array_intq)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  relu_1_bias, AI_STATIC,
  16, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &relu_1_bias_array, NULL)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  relu_1_output, AI_STATIC,
  17, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 8, 8), AI_STRIDE_INIT(4, 1, 1, 64, 512),
  1, &relu_1_output_array, &relu_1_output_array_intq)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  relu_1_scratch0, AI_STATIC,
  18, 0x0,
  AI_SHAPE_INIT(4, 1, 256, 1, 1), AI_STRIDE_INIT(4, 1, 1, 256, 256),
  1, &relu_1_scratch0_array, NULL)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  relu_1_weights, AI_STATIC,
  19, 0x1,
  AI_SHAPE_INIT(4, 64, 1, 1, 64), AI_STRIDE_INIT(4, 1, 64, 4096, 4096),
  1, &relu_1_weights_array, &relu_1_weights_array_intq)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_3_output, AI_STATIC,
  4, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 4, 4), AI_STRIDE_INIT(4, 1, 1, 64, 256),
  1, &conv2d_3_output_array, &conv2d_3_output_array_intq)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_3_scratch0, AI_STATIC,
  5, 0x0,
  AI_SHAPE_INIT(4, 1, 1729, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1729, 1729),
  1, &conv2d_3_scratch0_array, NULL)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_3_weights, AI_STATIC,
  6, 0x1,
  AI_SHAPE_INIT(4, 64, 3, 3, 1), AI_STRIDE_INIT(4, 1, 64, 64, 192),
  1, &conv2d_3_weights_array, &conv2d_3_weights_array_intq)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  relu_2_bias, AI_STATIC,
  21, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &relu_2_bias_array, NULL)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  relu_2_output, AI_STATIC,
  22, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 4, 4), AI_STRIDE_INIT(4, 1, 1, 64, 256),
  1, &relu_2_output_array, &relu_2_output_array_intq)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  relu_2_scratch0, AI_STATIC,
  23, 0x0,
  AI_SHAPE_INIT(4, 1, 256, 1, 1), AI_STRIDE_INIT(4, 1, 1, 256, 256),
  1, &relu_2_scratch0_array, NULL)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  relu_2_weights, AI_STATIC,
  24, 0x1,
  AI_SHAPE_INIT(4, 64, 1, 1, 64), AI_STRIDE_INIT(4, 1, 64, 4096, 4096),
  1, &relu_2_weights_array, &relu_2_weights_array_intq)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  mean_output, AI_STATIC,
  14, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &mean_output_array, NULL)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  relu_2_0_0_mean_conversion_output, AI_STATIC,
  20, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 4, 4), AI_STRIDE_INIT(4, 4, 4, 256, 1024),
  1, &relu_2_0_0_mean_conversion_output_array, NULL)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  relu_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &mfcc_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &relu_weights, &relu_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  relu_layer, 13,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSUA,
  &relu_chain,
  NULL, &relu_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(2, 2), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  conv2d_1_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_1_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &conv2d_1_weights, &conv2d_1_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_1_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  conv2d_1_layer, 16,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSUA,
  &conv2d_1_chain,
  NULL, &conv2d_1_layer, AI_STATIC, 
  .groups = 64, 
  .filter_stride = AI_SHAPE_2D_INIT(2, 2), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 1, 1, 1, 1), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_SAME, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  relu_1_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_1_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_1_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &relu_1_weights, &relu_1_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_1_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  relu_1_layer, 19,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSUA,
  &relu_1_chain,
  NULL, &relu_1_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  conv2d_3_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_1_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_3_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &conv2d_3_weights, &conv2d_1_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_3_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  conv2d_3_layer, 22,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSUA,
  &conv2d_3_chain,
  NULL, &conv2d_3_layer, AI_STATIC, 
  .groups = 64, 
  .filter_stride = AI_SHAPE_2D_INIT(2, 2), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 1, 1, 1, 1), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_SAME, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  relu_2_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_3_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_2_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &relu_2_weights, &relu_2_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_2_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  relu_2_layer, 25,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSUA,
  &relu_2_chain,
  NULL, &relu_2_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)


AI_STATIC_CONST ai_float mean_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    mean_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    mean_neutral_value_data, mean_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  mean_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_2_0_0_mean_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &mean_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  mean_layer, 28,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &mean_chain,
  NULL, &mean_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &mean_neutral_value, 
)
/**  Hybrid layers declarations section  *************************************/
void forward_lite_conv2d_integer_SSUA_relu(_stai_network_context* net_ctx)
{
  mfcc_output_array.data = AI_PTR(net_ctx->_inputs[0] + 0);
  mfcc_output_array.data_start = AI_PTR(net_ctx->_inputs[0] + 0);
  relu_weights_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  relu_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  relu_bias_array.data = AI_PTR(net_ctx->_weights[0] + 2560);
  relu_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 2560);
  relu_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 16608);
  relu_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 16608);
  relu_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  relu_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(13, 1, { mfcc_output.data->data});
  forward_conv2d_integer_SSUA(&relu_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(13, 1, { relu_output.data->data});
}
void forward_lite_conv2d_integer_SSUA_conv2d_1(_stai_network_context* net_ctx)
{
  relu_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  relu_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  conv2d_1_weights_array.data = AI_PTR(net_ctx->_weights[0] + 2816);
  conv2d_1_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 2816);
  conv2d_1_bias_array.data = AI_PTR(net_ctx->_weights[0] + 3392);
  conv2d_1_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 3392);
  conv2d_1_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  conv2d_1_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  conv2d_1_output_array.data = AI_PTR(net_ctx->_activations[0] + 17092);
  conv2d_1_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17092);
  _STAI_NETWORK_EVENT_NODE_START_CB(16, 1, { relu_output.data->data});
  forward_conv2d_integer_SSUA(&conv2d_1_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(16, 1, { conv2d_1_output.data->data});
}
void forward_lite_conv2d_integer_SSUA_relu_1(_stai_network_context* net_ctx)
{
  conv2d_1_output_array.data = AI_PTR(net_ctx->_activations[0] + 17092);
  conv2d_1_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17092);
  relu_1_weights_array.data = AI_PTR(net_ctx->_weights[0] + 3648);
  relu_1_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 3648);
  relu_1_bias_array.data = AI_PTR(net_ctx->_weights[0] + 7744);
  relu_1_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 7744);
  relu_1_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  relu_1_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  relu_1_output_array.data = AI_PTR(net_ctx->_activations[0] + 256);
  relu_1_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 256);
  _STAI_NETWORK_EVENT_NODE_START_CB(19, 1, { conv2d_1_output.data->data});
  forward_conv2d_integer_SSUA(&relu_1_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(19, 1, { relu_1_output.data->data});
}
void forward_lite_conv2d_integer_SSUA_conv2d_3(_stai_network_context* net_ctx)
{
  relu_1_output_array.data = AI_PTR(net_ctx->_activations[0] + 256);
  relu_1_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 256);
  conv2d_3_weights_array.data = AI_PTR(net_ctx->_weights[0] + 8000);
  conv2d_3_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 8000);
  conv2d_1_bias_array.data = AI_PTR(net_ctx->_weights[0] + 3392);
  conv2d_1_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 3392);
  conv2d_3_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 4352);
  conv2d_3_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 4352);
  conv2d_3_output_array.data = AI_PTR(net_ctx->_activations[0] + 6084);
  conv2d_3_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6084);
  _STAI_NETWORK_EVENT_NODE_START_CB(22, 1, { relu_1_output.data->data});
  forward_conv2d_integer_SSUA(&conv2d_3_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(22, 1, { conv2d_3_output.data->data});
}
void forward_lite_conv2d_integer_SSUA_relu_2(_stai_network_context* net_ctx)
{
  conv2d_3_output_array.data = AI_PTR(net_ctx->_activations[0] + 6084);
  conv2d_3_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6084);
  relu_2_weights_array.data = AI_PTR(net_ctx->_weights[0] + 8576);
  relu_2_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 8576);
  relu_2_bias_array.data = AI_PTR(net_ctx->_weights[0] + 12672);
  relu_2_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 12672);
  relu_2_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  relu_2_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  relu_2_output_array.data = AI_PTR(net_ctx->_activations[0] + 256);
  relu_2_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 256);
  _STAI_NETWORK_EVENT_NODE_START_CB(25, 1, { conv2d_3_output.data->data});
  forward_conv2d_integer_SSUA(&relu_2_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(25, 1, { relu_2_output.data->data});
}
void forward_lite_reduce_mean(_stai_network_context* net_ctx)
{
  relu_2_0_0_mean_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 1280);
  relu_2_0_0_mean_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 1280);
  mean_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  mean_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(28, 1, { relu_2_0_0_mean_conversion_output.data->data});
  forward_reduce(&mean_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(28, 1, { mean_output.data->data});
}

/*****************************************************************************/







static const ai_u32 relu_2_0_0_mean_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 1024;
static const ai_float relu_2_0_0_mean_conversion_t_in_0_fmt_scale_const_f32 = 0.027722682803869247f;
static const ai_u8 relu_2_0_0_mean_conversion_t_in_0_fmt_zero_const_u8 = 0;




static const ai_u32 logits_QuantizeLinear_Input_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4;
static const ai_float logits_QuantizeLinear_Input_0_conversion_t_out_0_fmt_scale_const_f32 = 0.04167867824435234f;
static const ai_u8 logits_QuantizeLinear_Input_0_conversion_t_out_0_fmt_zero_const_u8 = 106;
STAI_API_ENTRY
stai_return_code stai_network_run(
  stai_network* network,
  const stai_run_mode mode)
{
   STAI_UNUSED(mode)
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_ACTIVATIONS) != STAI_FLAG_ACTIVATIONS,
        STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_INPUTS) != STAI_FLAG_INPUTS,
                  STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_OUTPUTS) != STAI_FLAG_OUTPUTS,
                  STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_WEIGHTS) != STAI_FLAG_WEIGHTS,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)


  /* LITE_KERNEL_SECTION BEGIN relu */
  {
    
  forward_lite_conv2d_integer_SSUA_relu(net_ctx);
  }
  /* LITE_KERNEL_SECTION END relu */
  /* LITE_KERNEL_SECTION BEGIN conv2d_1 */
  {
    
  forward_lite_conv2d_integer_SSUA_conv2d_1(net_ctx);
  }
  /* LITE_KERNEL_SECTION END conv2d_1 */
  /* LITE_KERNEL_SECTION BEGIN relu_1 */
  {
    
  forward_lite_conv2d_integer_SSUA_relu_1(net_ctx);
  }
  /* LITE_KERNEL_SECTION END relu_1 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_3 */
  {
    
  forward_lite_conv2d_integer_SSUA_conv2d_3(net_ctx);
  }
  /* LITE_KERNEL_SECTION END conv2d_3 */
  /* LITE_KERNEL_SECTION BEGIN relu_2 */
  {
    
  forward_lite_conv2d_integer_SSUA_relu_2(net_ctx);
  }
  /* LITE_KERNEL_SECTION END relu_2 */
  /* LITE_KERNEL_SECTION BEGIN relu_2_0_0_mean_conversion */
  {
      const ai_u8* relu_2_0_0_mean_conversion_t_in_0_ptr_const_u8 = (ai_u8*)(net_ctx->_activations[0] + 256);
    ai_float* relu_2_0_0_mean_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 1280);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(25, 1, {(stai_ptr) relu_2_0_0_mean_conversion_t_in_0_ptr_const_u8});
    
  forward_lite_node_convert_integer_iu8of32(relu_2_0_0_mean_conversion_t_in_0_ptr_const_u8, relu_2_0_0_mean_conversion_t_out_0_ptr_f32, relu_2_0_0_mean_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, relu_2_0_0_mean_conversion_t_in_0_fmt_scale_const_f32, relu_2_0_0_mean_conversion_t_in_0_fmt_zero_const_u8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(25, 1, {(stai_ptr) relu_2_0_0_mean_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END relu_2_0_0_mean_conversion */
  /* LITE_KERNEL_SECTION BEGIN mean */
  {
    
  forward_lite_reduce_mean(net_ctx);
  }
  /* LITE_KERNEL_SECTION END mean */
  /* LITE_KERNEL_SECTION BEGIN mean_Mul */
  {
      ai_float* mean_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 256);
    const ai_float* mean_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* mean_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 12928);
    const ai_float* mean_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 13184);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(28, 1, {(stai_ptr) mean_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(mean_Mul_t_out_0_ptr_f32, mean_Mul_t_in_0_ptr_const_f32, mean_Mul_t_weight_0_ptr_const_f32, mean_Mul_t_weight_1_ptr_const_f32, (ai_u32)(64), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(28, 1, {(stai_ptr) mean_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END mean_Mul */
  /* LITE_KERNEL_SECTION BEGIN logits_QuantizeLinear_Input */
  {
      forward_lite_dense_if32of32wf32_args arg_30f51e = {
      .output = (float*)(net_ctx->_activations[0] + 0),
      .input = (float*)(net_ctx->_activations[0] + 256),
      .weights = (float*)(net_ctx->_weights[0] + 13440),
      .bias = (float*)(net_ctx->_weights[0] + 14464),
      .n_channel_in = 64,
      .n_channel_out = 4,
      .n_elements = 1,
    };
  
  _STAI_NETWORK_EVENT_NODE_START_CB(32, 1, {(stai_ptr) (float*)(net_ctx->_activations[0] + 256)});
    
  forward_lite_dense_if32of32wf32((forward_lite_dense_if32of32wf32_args*)&arg_30f51e);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(32, 1, {(stai_ptr) (float*)(net_ctx->_activations[0] + 0)});
  }
  /* LITE_KERNEL_SECTION END logits_QuantizeLinear_Input */
  /* LITE_KERNEL_SECTION BEGIN logits_QuantizeLinear_Input_0_conversion */
  {
      const ai_float* logits_QuantizeLinear_Input_0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_u8* logits_QuantizeLinear_Input_0_conversion_t_out_0_ptr_u8 = (ai_u8*)(net_ctx->_outputs[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(32, 1, {(stai_ptr) logits_QuantizeLinear_Input_0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32ou8(logits_QuantizeLinear_Input_0_conversion_t_in_0_ptr_const_f32, logits_QuantizeLinear_Input_0_conversion_t_out_0_ptr_u8, logits_QuantizeLinear_Input_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, logits_QuantizeLinear_Input_0_conversion_t_out_0_fmt_scale_const_f32, logits_QuantizeLinear_Input_0_conversion_t_out_0_fmt_zero_const_u8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(32, 1, {(stai_ptr) logits_QuantizeLinear_Input_0_conversion_t_out_0_ptr_u8});
  }
  /* LITE_KERNEL_SECTION END logits_QuantizeLinear_Input_0_conversion */
  return net_ctx->_return_code;
}

/*****************************************************************************/
/*  Getters APIs Section  */
STAI_API_ENTRY
stai_size stai_network_get_context_size()
{
  return (stai_size)STAI_NETWORK_CONTEXT_SIZE;
}

#if defined(HAVE_NETWORK_INFO)
STAI_API_ENTRY
stai_return_code stai_network_get_info(
  stai_network* network,
  stai_network_info* info)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, info==NULL, STAI_ERROR_NETWORK_INVALID_INFO, net_ctx->_return_code)

  // Copy of network info struct
  *info = g_network_info;

  return STAI_SUCCESS;
}
#endif


STAI_API_ENTRY
stai_return_code stai_network_get_activations(
  stai_network* network, stai_ptr* activations, stai_size* n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, !n_activations, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_activations = STAI_NETWORK_ACTIVATIONS_NUM;
for (stai_size idx=0; activations && (idx<STAI_NETWORK_ACTIVATIONS_NUM); idx++) {
    // get address of the activations buffers
    activations[idx] = net_ctx->_activations[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_weights(
  stai_network* network, stai_ptr* weights, stai_size* n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_weights, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_weights = STAI_NETWORK_WEIGHTS_NUM;
for (stai_size idx=0; weights && (idx<STAI_NETWORK_WEIGHTS_NUM); idx++) {
    // get address of the weights buffers
    weights[idx] = net_ctx->_weights[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_inputs(
  stai_network* network, stai_ptr* inputs, stai_size* n_inputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_inputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_inputs = STAI_NETWORK_IN_NUM;
  for (stai_size idx=0; inputs && (idx<STAI_NETWORK_IN_NUM); idx++) {
    inputs[idx] = net_ctx->_inputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_outputs(
  stai_network* network, stai_ptr* outputs, stai_size* n_outputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_outputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_outputs = STAI_NETWORK_OUT_NUM;
  for (stai_size idx=0; outputs && (idx<STAI_NETWORK_OUT_NUM); idx++) {
    outputs[idx] = net_ctx->_outputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_error(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /* return 1st generated error or STAI_SUCCESS if no errors so far */
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_states(
  stai_network* network, stai_ptr* states, stai_size* n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_states, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  /* get the number of internals states (supporting multi-heap also for internal states) */
  *n_states = STAI_NETWORK_STATES_NUM;

  STAI_UNUSED(states)
return net_ctx->_return_code;
}


/*****************************************************************************/
/*  Setters APIs Section  */

STAI_API_ENTRY
stai_return_code stai_network_set_activations(
  stai_network* network,
  const stai_ptr* activations,
  const stai_size n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _activations_alignment[] = STAI_NETWORK_ACTIVATIONS_ALIGNMENTS;
  STAI_PRINT("  [stai_network_set_activations] network(%p) activations[%d]: %p\n\n", net_ctx, n_activations, activations)
  _STAI_SET_ERROR(net_ctx, !activations,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_activations!=STAI_NETWORK_ACTIVATIONS_NUM,
                  STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_NUM, net_ctx->_return_code)

  for (stai_size idx=0; activations && idx<STAI_NETWORK_ACTIVATIONS_NUM; idx++) {
    STAI_PRINT("  activation[%d]: %p\n", idx, activations[idx])
    _STAI_SET_ERROR(net_ctx, activations[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)activations[idx]) & (_activations_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_activations[idx] = activations[idx];
  }
  net_ctx->_inputs[0] = activations[0] + 15328;

  net_ctx->_outputs[0] = activations[0] + 16;
_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_weights(
  stai_network* network,
  const stai_ptr* weights,
  const stai_size n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _weights_alignment[] = STAI_NETWORK_WEIGHTS_ALIGNMENTS;
  _STAI_SET_ERROR(net_ctx, !weights,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_weights!=STAI_NETWORK_WEIGHTS_NUM,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_NUM, net_ctx->_return_code)
  for (stai_size idx=0; weights && idx<STAI_NETWORK_WEIGHTS_NUM; idx++) {
    STAI_PRINT("  weight[%d]: %p\n", idx, weights[idx])
    _STAI_SET_ERROR(net_ctx, weights[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)weights[idx]) & (_weights_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_weights[idx] = weights[idx];
  }_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_inputs(
  stai_network* network,
  const stai_ptr* inputs,
  const stai_size n_inputs)
{
  const uintptr_t _inputs_alignment[] = STAI_NETWORK_IN_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !inputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_inputs!=STAI_NETWORK_IN_NUM,
                  STAI_ERROR_NETWORK_INVALID_IN_NUM, net_ctx->_return_code)

  for (stai_size idx=0; inputs && idx<STAI_NETWORK_IN_NUM; idx++) {
    STAI_PRINT("  input[%d]: %p\n", idx, inputs[idx])
    _STAI_SET_ERROR(net_ctx, inputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)inputs[idx]) & (_inputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_inputs[idx] = inputs[idx];
  }

  _stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_outputs(
  stai_network* network,
  const stai_ptr* outputs,
  const stai_size n_outputs)
{
  const uintptr_t _outputs_alignment[] = STAI_NETWORK_OUT_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !outputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_outputs!=STAI_NETWORK_OUT_NUM,
                  STAI_ERROR_NETWORK_INVALID_OUT_NUM, net_ctx->_return_code)

  for (stai_size idx=0; outputs && idx<n_outputs; idx++) {
    STAI_PRINT("  output[%d]: %p\n", idx, outputs[idx])
    _STAI_SET_ERROR(net_ctx, outputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)outputs[idx]) & (_outputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_outputs[idx] = outputs[idx];
  }

  _stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_states(
  stai_network* network,
  const stai_ptr* states,
  const stai_size n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  STAI_UNUSED(states)
  STAI_UNUSED(n_states)
_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}

STAI_API_ENTRY
stai_return_code stai_network_set_callback(
  stai_network* network, const stai_event_cb cb, void* cb_cookie)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  STAI_PRINT("  set_callback %p cb %p cookie %p\n", net_ctx, cb, cb_cookie)
  // _STAI_SET_ERROR(net_ctx, cb==NULL, STAI_ERROR_NETWORK_INVALID_CALLBACK, net_ctx->_return_code)
  net_ctx->_callback = cb;
  net_ctx->_callback_cookie = cb_cookie;
  return net_ctx->_return_code;
}

#undef _STAI_SET_ERROR
#undef _STAI_CONTEXT_ALIGNMENT
#undef _STAI_CONTEXT_ACQUIRE
#undef _STAI_NETWORK_EVENT_NODE_START_CB
#undef _STAI_NETWORK_EVENT_NODE_STOP_CB
#undef _STAI_NETWORK_MODEL_SIGNATURE
#undef _STAI_NETWORK_DATETIME
#undef _STAI_NETWORK_COMPILE_DATETIME

