#pragma once
#include <stdint.h>
void rtc_set_unix(uint32_t t);
void rtc_get_hms(uint8_t *h, uint8_t *m, uint8_t *s);
