/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "relay.h"

#include "all_device_types_app.h"
#include "app_storage.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "relay";

#define STORAGE_KEY_ON_OFF      "onoff"
#define STORAGE_KEY_STARTUP     "startup"

static void relay_drive(bool on)
{
    gpio_set_level(RELAY_GPIO, on ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL);
}

void relay_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << RELAY_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    relay_drive(false);
    gpio_config(&cfg);
    relay_drive(false);
}

void relay_set(bool on)
{
    relay_drive(on);
    app_storage_set_u8(STORAGE_KEY_ON_OFF, on ? 1 : 0);

    ESP_LOGI(TAG, "Relay %s", on ? "ON" : "OFF");
}

uint8_t relay_get_startup_on_off(void)
{
    return app_storage_get_u8(STORAGE_KEY_STARTUP,
                              EZB_ZCL_ON_OFF_START_UP_ON_OFF_SET_PREVIOUS_ON_OFF);
}

void relay_set_startup_on_off(uint8_t value)
{
    bool valid = value <= EZB_ZCL_ON_OFF_START_UP_ON_OFF_TOGGLE_PREVIOUS_ON_OFF ||
                 value == EZB_ZCL_ON_OFF_START_UP_ON_OFF_SET_PREVIOUS_ON_OFF;

    if (!valid) {
        ESP_LOGW(TAG, "Ignoring invalid StartUpOnOff 0x%02x", value);
        return;
    }

    app_storage_set_u8(STORAGE_KEY_STARTUP, value);

    ESP_LOGI(TAG, "StartUpOnOff set to 0x%02x", value);
}

bool relay_initial_state(uint8_t startup_on_off)
{
    bool last_on = app_storage_get_u8(STORAGE_KEY_ON_OFF, 0) != 0;

    switch (startup_on_off) {
    case EZB_ZCL_ON_OFF_START_UP_ON_OFF_SET_ON_OFF_TO_OFF:
        return false;
    case EZB_ZCL_ON_OFF_START_UP_ON_OFF_SET_ON_OFF_TO_ON:
        return true;
    case EZB_ZCL_ON_OFF_START_UP_ON_OFF_TOGGLE_PREVIOUS_ON_OFF:
        return !last_on;
    default:
        return last_on;
    }
}
