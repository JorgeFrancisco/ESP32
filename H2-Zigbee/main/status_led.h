/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

/*
 * Status LED (WS2812 on the ESP32-H2-DevKitM-1).
 *
 *   OFF           off                no network, waiting for the BOOT button
 *   PAIRING       blue, blinking     searching for a network
 *   JOINED        purple, blinking   joined, coordinator interview in progress
 *   INTERVIEWED   purple, solid      interview complete
 *   COMMISSIONED  green, solid       configured/used by the coordinator
 *
 * Errors override the state: solid red for a few seconds after leaving the
 * network, blinking red when the interview does not complete in time.
 */
typedef enum {
    STATUS_LED_OFF = 0,
    STATUS_LED_PAIRING,
    STATUS_LED_JOINED,
    STATUS_LED_INTERVIEWED,
    STATUS_LED_COMMISSIONED,
} status_led_state_t;

void status_led_init(void);

void status_led_set_state(status_led_state_t state);

status_led_state_t status_led_get_state(void);

/* Shows solid red for STATUS_LED_ERROR_MS, then goes back to the current state */
void status_led_flash_error(void);
