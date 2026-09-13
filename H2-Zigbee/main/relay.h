/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Configures the relay GPIO with the relay off (no pulse during boot) */
void relay_init(void);

/* Drives the relay and stores the state as "last state" */
void relay_set(bool on);

/* StartUpOnOff (ZCL 0x4003): 0x00 off, 0x01 on, 0x02 toggle last, 0xFF last state */
uint8_t relay_get_startup_on_off(void);

void relay_set_startup_on_off(uint8_t value);

/* Relay state to apply at boot, according to StartUpOnOff and the last state */
bool relay_initial_state(uint8_t startup_on_off);
