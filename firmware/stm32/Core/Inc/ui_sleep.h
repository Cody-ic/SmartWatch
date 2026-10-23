/**
 * @file   ui_sleep.h
 * @brief  综合睡眠页面 — 睡眠分期 + 呼吸暂停初筛
 *
 * [2026-07-05] 新增, 合并 sleep_staging + sleep_apnea 信息
 */
#ifndef UI_SLEEP_H
#define UI_SLEEP_H

#include "lvgl.h"

void ui_sleep_create(lv_obj_t *parent);
void ui_sleep_update(void);

#endif
