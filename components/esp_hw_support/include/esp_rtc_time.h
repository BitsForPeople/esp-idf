/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get current value of RTC counter in microseconds
 *
 * Note: this function may take up to 1 RTC_SLOW_CLK cycle to execute
 *
 * @return current value of RTC counter in microseconds
 */
uint64_t esp_rtc_get_time_us(void);

#ifdef __cplusplus
}
#endif
