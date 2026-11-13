/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "status_led.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "status_led";

/*
 * Each pattern is one second long: STATUS_LED_PATTERN_STEPS steps of
 * STATUS_LED_STEP_MS, bit n set = LED on during step n.
 */
#define STATUS_LED_PATTERN_STEPS    10

#define PATTERN_OFF                 0x000u  /* .......... */
#define PATTERN_ON                  0x3FFu  /* ########## */
#define PATTERN_SLOW_BLINK          0x01Fu  /* #####..... */
#define PATTERN_FAST_BLINK          0x155u  /* #.#.#.#.#. */
#define PATTERN_DOUBLE_FLASH        0x005u  /* #.#....... */
#define PATTERN_SHORT_FLASH         0x001u  /* #......... */

static const uint16_t s_state_patterns[] = {
    [STATUS_LED_OFF]           = PATTERN_OFF,
    [STATUS_LED_ADVERTISING]   = PATTERN_SLOW_BLINK,
    [STATUS_LED_COMMISSIONING] = PATTERN_FAST_BLINK,
    [STATUS_LED_ATTACHING]     = PATTERN_ON,
    [STATUS_LED_CONNECTED]     = PATTERN_ON,
};

static volatile status_led_state_t s_state = STATUS_LED_OFF;
static volatile TickType_t s_error_until;

static void led_write(bool on)
{
    gpio_set_level(STATUS_LED_GPIO, on ? STATUS_LED_ACTIVE_LEVEL : !STATUS_LED_ACTIVE_LEVEL);
}

/*
 * A recent error or a Thread network that takes too long overrides the state.
 * Once connected for a while, the LED turns off (the relay has its own LED).
 */
static uint16_t led_current_pattern(status_led_state_t state, TickType_t now, TickType_t in_state)
{
    if (s_error_until != 0 && now < s_error_until) {
        return PATTERN_DOUBLE_FLASH;
    }

    if (state == STATUS_LED_ATTACHING && in_state > pdMS_TO_TICKS(STATUS_LED_ATTACH_TIMEOUT_MS)) {
        return PATTERN_SHORT_FLASH;
    }

    if (state == STATUS_LED_CONNECTED && in_state >= pdMS_TO_TICKS(STATUS_LED_CONNECTED_MS)) {
        return PATTERN_OFF;
    }

    return s_state_patterns[state];
}

static void status_led_task(void *arg)
{
    (void)arg;

    status_led_state_t last_state = s_state;
    TickType_t state_since = xTaskGetTickCount();
    uint16_t last_pattern = PATTERN_OFF;
    unsigned step = 0;

    for (;;) {
        TickType_t now = xTaskGetTickCount();
        status_led_state_t state = s_state;

        if (state != last_state) {
            last_state = state;
            state_since = now;
        }

        uint16_t pattern = led_current_pattern(state, now, now - state_since);

        /* Start a new pattern from its first step, so it is recognizable at once */
        if (pattern != last_pattern) {
            last_pattern = pattern;
            step = 0;
        }

        led_write((pattern >> step) & 1u);
        step = (step + 1) % STATUS_LED_PATTERN_STEPS;

        vTaskDelay(pdMS_TO_TICKS(STATUS_LED_STEP_MS));
    }
}

void status_led_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << STATUS_LED_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    if (gpio_config(&cfg) != ESP_OK) {
        ESP_LOGW(TAG, "Status LED unavailable (GPIO init failed)");
        return;
    }

    led_write(false);
    xTaskCreate(status_led_task, "status_led", 2048, NULL, 3, NULL);
}

void status_led_set_state(status_led_state_t state)
{
    s_state = state;
}

void status_led_flash_error(void)
{
    s_error_until = xTaskGetTickCount() + pdMS_TO_TICKS(STATUS_LED_ERROR_MS);
}
