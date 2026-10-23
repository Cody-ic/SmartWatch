#ifndef SPU0410_H
#define SPU0410_H

#include <stdint.h>
#include "stm32u5xx_hal.h"

#define MIC_BUFFER_SIZE 256

// 喊叫检测阈值
#define SHOUT_ENERGY_THRESH  8000
#define SHOUT_ZCR_MIN        0.12f
#define SHOUT_ZCR_MAX        0.45f
#define SHOUT_CONTINUOUS_CNT 2

#define MIC_ADC_RESOLUTION 4096.0f
#define MIC_VREF           3.3f

int  mic_init(void);
void mic_start(void);
void mic_stop(void);
int  mic_is_frame_ready(void);
void mic_clear_frame_flag(void);
uint32_t mic_get_energy(void);
float    mic_get_zcr(void);
int      mic_is_shouting(void);

/**
 * [2026-07-04]: 供 KWS 任务获取原始音频帧数据
 * @param buf  输出缓冲区 (MIC_BUFFER_SIZE × int16)
 * @return 0=成功, -1=帧未就绪
 */
int mic_get_frame(int16_t *buf);

/* [2026-07-11]: KWS 启动后调用, 激活 KWS 独立消费通道(方案C双缓冲两独立 SPSC) */
void mic_set_kws_active(uint8_t on);

#endif
