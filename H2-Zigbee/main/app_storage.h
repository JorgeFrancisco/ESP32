/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>

/* Small persistent key/value store (NVS namespace "app") */

uint8_t app_storage_get_u8(const char *key, uint8_t default_value);

/* Writes only when the stored value differs, to spare the flash */
void app_storage_set_u8(const char *key, uint8_t value);
