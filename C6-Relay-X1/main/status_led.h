/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Status LED: red programmable LED of the ESP32-C6-Relay_X1 (GPIO2).
 * The relay has its own indicator LED on the board, so this LED only shows the
 * network status, with blink patterns instead of colors:
 *
 *   OFF            off                    not commissioned, commissioning window closed
 *   ADVERTISING    slow blink (1 Hz)      waiting to be commissioned (Bluetooth LE)
 *   COMMISSIONING  fast blink (5 Hz)      commissioning in progress
 *   ATTACHING      on                     commissioned, joining the Thread network
 *   CONNECTED      on, then off           attached to the Thread network; the LED
 *                                         turns off after STATUS_LED_CONNECTED_MS
 *
 * Errors override the state: a double flash every second for a few seconds
 * after a failed commissioning, a removal or a factory reset; a short flash
 * every second when the Thread network is not reached in time.
 */
typedef enum {
    STATUS_LED_OFF = 0,
    STATUS_LED_ADVERTISING,
    STATUS_LED_COMMISSIONING,
    STATUS_LED_ATTACHING,
    STATUS_LED_CONNECTED,
} status_led_state_t;

void status_led_init(void);

void status_led_set_state(status_led_state_t state);

/* Shows the error pattern for STATUS_LED_ERROR_MS, then goes back to the current state */
void status_led_flash_error(void);

#ifdef __cplusplus
}
#endif
