/**
 * @file ui_adaptive.h
 * @brief 第5页：AI 原型网络自适应
 *
 * 显示内容：
 *   1. 当前在线学习状态（OFF / COLLECT / READY / UPDATE）
 *   2. 实时置信度阈值 / SVM 间隔
 *   3. 各类别已采集支持样本数（7 类 HAR）
 *   4. 累计模型更新次数 / 最近更新时间
 *   5. "开始校准" 按钮（短按进入数据采集模式）
 *
 * 与原型网络引擎（external）解耦：本模块只负责显示，
 * 由外部 adaptive_engine 调用 ui_adaptive_update() 推送数据。
 *
 * © 2026 AI Watch Project
 */
#ifndef UI_ADAPTIVE_H
#define UI_ADAPTIVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui.h"

/* ---------- 自适应状态机 ---------- */
typedef enum {
    ADAPT_OFF = 0,       /* 未启用 */
    ADAPT_COLLECT,       /* 采集中（用户在为新类别提供样本） */
    ADAPT_READY,         /* 已收集足够样本，待更新 */
    ADAPT_UPDATING,      /* 正在更新原型 */
    ADAPT_UPDATED,       /* 更新完成（5 秒后回 OFF） */
} adaptive_state_t;

/* ---------- 公共 API ---------- */

/**
 * 创建第5页内容到给定 tile 上
 * 在 ui_init() 里被 ui.c 调用
 */
void ui_adaptive_create(lv_obj_t *tile);

/**
 * 由 adaptive_engine 推送状态更新
 *
 * @param state          当前自适应状态
 * @param confidence     当前置信度阈值（0.0-1.0，显示为百分比）
 * @param margin         SVM/原型间隔（0.0-1.0，可显示余量）
 * @param support_counts 7 类 HAR 各自的支持样本数（int[7]）
 * @param updates        累计模型更新次数
 * @param last_update_min 最近更新距今分钟数（-1 表示从未）
 */
void ui_adaptive_update(adaptive_state_t state,
                        float confidence,
                        float margin,
                        const int *support_counts,
                        int updates,
                        int last_update_min);

/**
 * 用户按下"开始校准"按钮的回调（弱定义）
 * 工程层可重写以触发数据采集
 */
void ui_adaptive_on_calibrate(void) __attribute__((weak));

#ifdef __cplusplus
}
#endif
#endif /* UI_ADAPTIVE_H */
