/**
 * @file task_voice_guide.h
 * @brief 语音实时指导模块 — 运动状态驱动的语音反馈
 *
 * 产品定位：
 *   低成本实现高端手表做不到的功能 — 实时语音指导
 *   目标用户：老年人、运动初学者
 *
 * 设计原则：
 *   - 预录制音频片段（不在线 TTS），存 Flash
 *   - DAC + PWM 或 SPI DAC 播放
 *   - 基于 HAR + HR 的智能触发（不频繁打断）
 *   - 每种指导最少间隔 30 秒（防烦人）
 *
 * 触发条件：
 *   1. 心率过高 → "心率偏高，请放慢速度"
 *   2. 心率太低（有效运动判定）→ "请加快运动节奏"
 *   3. 运动效果达标 → "运动效果良好，继续保持"
 *   4. 久坐提醒（>30min sitting）→ "久坐提醒，起来活动一下"
 *   5. 有效运动完成 → "已达到今日运动目标"
 */

#ifndef TASK_VOICE_GUIDE_H
#define TASK_VOICE_GUIDE_H

#include <stdint.h>
#include "app_config.h"

/* ======================== 指导类型 ======================== */

typedef enum {
    VOICE_NONE = 0,

    /* 心率相关 */
    VOICE_HR_TOO_HIGH,       /* 心率偏高提醒 */
    VOICE_HR_TOO_LOW,        /* 心率偏低提醒 */

    /* 运动指导 */
    VOICE_SPEED_UP,          /* 加速指导 */
    VOICE_SLOW_DOWN,         /* 减速指导 */
    VOICE_KEEP_GOING,        /* 保持节奏 */

    /* 久坐提醒 */
    VOICE_SITTING_TOO_LONG,  /* 久坐提醒 */

    /* 运动目标 */
    VOICE_GOAL_REACHED,      /* 目标达成 */

    VOICE_COUNT
} voice_guide_type_t;

/* ======================== 配置参数 ======================== */

/* 指导间隔（同类型） */
#define VOICE_MIN_INTERVAL_S       30     /* 同类型最少 30 秒间隔 */
#define VOICE_GLOBAL_MIN_INTERVAL_S 15    /* 全局最少 15 秒间隔 */

/* 久坐提醒阈值 */
#define SITTING_REMINDER_S         1800    /* 30 分钟 */

/* 心率区间定义（基于 Karvonen 公式） */
/* HR_reserve = HR_max - HR_resting */
/* Zone 1: 50-60% → 热身 */
/* Zone 2: 60-70% → 脂肪燃烧 */
/* Zone 3: 70-80% → 有氧 */
/* Zone 4: 80-90% → 无氧 */
/* Zone 5: 90-100% → 极限 */

#define HR_ZONE_WARMUP_LO     0.50f
#define HR_ZONE_AEROBIC_LO    0.60f
#define HR_ZONE_AEROBIC_HI    0.80f
#define HR_ZONE_ANAEROBIC_LO  0.80f
#define HR_ZONE_DANGER_HI     0.90f

/* ======================== API ======================== */

/**
 * @brief 初始化语音指导模块
 */
void voice_guide_init(void);

/**
 * @brief 主任务循环（1Hz）
 * @param argument FreeRTOS 参数（未使用）
 */
void TaskVoiceGuide(void *argument);

/**
 * @brief 手动触发语音指导（外部调用）
 * @param type 指导类型
 */
void voice_trigger(voice_guide_type_t type);

#endif /* TASK_VOICE_GUIDE_H */
