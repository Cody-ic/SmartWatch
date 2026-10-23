/**
 * @file ui_spo2.h
 * @brief SpO2 血氧页面
 */
#ifndef UI_SPO2_H
#define UI_SPO2_H

#include "lvgl.h"

void ui_spo2_create(lv_obj_t *parent);
void ui_spo2_update(uint8_t bpm);

#endif
