/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "network.h"

#include "all_device_types_app.h"
#include "app_storage.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "status_led.h"

static const char *TAG = "network";

/* Key name kept for compatibility with devices already commissioned */
#define STORAGE_KEY_COMMISSIONED    "hub_confirmed"

static bool s_joined;
static bool s_pairing_active;
static bool s_local_reset_pending;   /* leave requested by the BOOT button */
static TickType_t s_pairing_deadline;
static TimerHandle_t s_retry_timer;

static bool s_scan_in_progress;
static bool s_scan_found_open;
static ezb_extpanid_t s_scan_open_extpanid;

/* -------------------------------------------------------------------------- */
/* Commissioning state                                                        */
/* -------------------------------------------------------------------------- */

static bool commissioned_get(void)
{
    return app_storage_get_u8(STORAGE_KEY_COMMISSIONED, 0) != 0;
}

static void commissioned_set(bool commissioned)
{
    app_storage_set_u8(STORAGE_KEY_COMMISSIONED, commissioned ? 1 : 0);
}

/* -------------------------------------------------------------------------- */
/* Network steering with retries                                              */
/* -------------------------------------------------------------------------- */

/* Restricts steering to one network; NULL allows any network */
static void select_network(const ezb_extpanid_t *extpanid)
{
    ezb_extpanid_t any = { 0 };

    ezb_set_use_extended_panid(extpanid != NULL ? extpanid : &any);
}

static void retry_timer_cb(TimerHandle_t timer)
{
    (void)timer;

    if (!s_pairing_active) {
        return;
    }

    /* Timer task: the Zigbee lock is required to call the stack */
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
        esp_zigbee_lock_release();
    }
}

static bool pairing_window_expired(void)
{
    if (xTaskGetTickCount() < s_pairing_deadline) {
        return false;
    }

    s_pairing_active = false;
    status_led_set_state(STATUS_LED_OFF);

    ESP_LOGW(TAG, "Pairing window closed without joining. Hold BOOT for %d s to retry",
             PAIRING_BUTTON_HOLD_MS / 1000);
    return true;
}

static void schedule_retry(void)
{
    if (!s_pairing_active || pairing_window_expired()) {
        return;
    }

    status_led_set_state(STATUS_LED_PAIRING);

    if (s_retry_timer == NULL) {
        s_retry_timer = xTimerCreate("zb_retry", pdMS_TO_TICKS(PAIRING_RETRY_MS),
                                     pdFALSE, NULL, retry_timer_cb);
    }

    if (s_retry_timer != NULL) {
        xTimerStart(s_retry_timer, 0);
    }
}

/* -------------------------------------------------------------------------- */
/* Scan for an open network                                                   */
/* -------------------------------------------------------------------------- */

/*
 * Many routers of another network on the same channel can make the stack miss
 * the beacons of the network that permits joining ("no network" failures).
 * Scanning first and pinning that network's extended PAN ID avoids it.
 */
static void scan_cb(ezb_nwk_active_scan_result_t *result, void *user_ctx)
{
    (void)user_ctx;

    if (result != NULL) {
        if (result->permit_join && !s_scan_found_open) {
            s_scan_open_extpanid = result->extpanid;
            s_scan_found_open = true;
        }
        return;
    }

    /* NULL result: scan finished */
    s_scan_in_progress = false;

    if (s_scan_found_open) {
        ESP_LOGI(TAG, "Open network found: extended PAN ID 0x%016llx",
                 s_scan_open_extpanid.u64);
        select_network(&s_scan_open_extpanid);
    } else {
        select_network(NULL);
    }

    schedule_retry();
}

static void scan_then_retry(void)
{
    if (s_scan_in_progress) {
        return;
    }

    const ezb_nwk_scan_req_t req = {
        .scan_type      = EZB_NWK_SCAN_TYPE_ACTIVE,
        .scan_duration  = 3,
        .scan_channels  = ESP_ZIGBEE_PRIMARY_CHANNEL_MASK,
        .active_scan_cb = scan_cb,
        .user_ctx       = NULL,
    };

    s_scan_found_open = false;
    s_scan_in_progress = true;

    if (ezb_nwk_scan(&req) != EZB_ERR_NONE) {
        ESP_LOGW(TAG, "Network scan failed to start");
        s_scan_in_progress = false;
        schedule_retry();
    }
}

/* -------------------------------------------------------------------------- */
/* Pairing mode                                                               */
/* -------------------------------------------------------------------------- */

static void pairing_start(bool scan_now)
{
    s_pairing_active = true;
    s_pairing_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(PAIRING_WINDOW_MS);

    select_network(NULL);
    status_led_set_state(STATUS_LED_PAIRING);

    ESP_LOGI(TAG, "Pairing mode for %d s", PAIRING_WINDOW_MS / 1000);

    if (scan_now) {
        scan_then_retry();
    } else {
        schedule_retry();
    }
}

/* Pairs from scratch, leaving the current network first. Needs the Zigbee lock. */
static void factory_reset_and_pair(void)
{
    commissioned_set(false);

    if (ezb_bdb_is_factory_new()) {
        pairing_start(true);
    } else {
        s_local_reset_pending = true;
        ezb_bdb_reset_via_local_action(); /* ends in network_on_left() */
    }
}

/* -------------------------------------------------------------------------- */
/* BOOT button                                                                */
/* -------------------------------------------------------------------------- */

static void button_task(void *arg)
{
    (void)arg;

    TickType_t pressed_since = 0;
    bool handled = false;

    for (;;) {
        bool pressed = gpio_get_level(PAIRING_BUTTON_GPIO) == 0;
        TickType_t now = xTaskGetTickCount();

        if (!pressed) {
            pressed_since = 0;
            handled = false;
        } else if (pressed_since == 0) {
            pressed_since = now | 1; /* 0 means released */
        } else if (!handled &&
                   now - pressed_since >= pdMS_TO_TICKS(PAIRING_BUTTON_HOLD_MS)) {
            handled = true;

            ESP_LOGI(TAG, "BOOT held for %d s: pairing from scratch",
                     PAIRING_BUTTON_HOLD_MS / 1000);

            if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
                factory_reset_and_pair();
                esp_zigbee_lock_release();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void network_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PAIRING_BUTTON_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    gpio_config(&cfg);
    xTaskCreate(button_task, "pair_btn", 3072, NULL, 4, NULL);
}

bool network_is_joined(void)
{
    return s_joined;
}

void network_on_stack_started(bool factory_new)
{
    if (factory_new) {
        status_led_set_state(STATUS_LED_OFF);
        ESP_LOGI(TAG, "No network. Hold BOOT for %d s to pair", PAIRING_BUTTON_HOLD_MS / 1000);
        return;
    }

    s_joined = true;
    status_led_set_state(commissioned_get() ? STATUS_LED_COMMISSIONED
                                            : STATUS_LED_INTERVIEWED);
}

void network_on_steering_done(uint8_t bdb_status)
{
    if (bdb_status == EZB_BDB_STATUS_SUCCESS) {
        s_joined = true;
        s_pairing_active = false;
        select_network(NULL);

        /* The interview may finish before this signal: never downgrade the LED */
        if (commissioned_get()) {
            status_led_set_state(STATUS_LED_COMMISSIONED);
        } else if (status_led_get_state() != STATUS_LED_INTERVIEWED) {
            status_led_set_state(STATUS_LED_JOINED);
        }
        return;
    }

    if (bdb_status == EZB_BDB_STATUS_NO_NETWORK && s_pairing_active) {
        scan_then_retry();
    } else {
        schedule_retry();
    }
}

void network_on_left(bool reset_without_rejoin)
{
    s_joined = false;

    if (!reset_without_rejoin) {
        return;
    }

    commissioned_set(false);

    if (s_local_reset_pending) {
        s_local_reset_pending = false;
    } else {
        ESP_LOGW(TAG, "Removed from the network (or failed to join)");
        status_led_flash_error();
    }

    pairing_start(false);
}

void network_on_interview_complete(void)
{
    if (commissioned_get() || status_led_get_state() == STATUS_LED_INTERVIEWED) {
        return;
    }

    ESP_LOGI(TAG, "Coordinator interview complete");
    status_led_set_state(STATUS_LED_INTERVIEWED);
}

void network_on_coordinator_configured(void)
{
    if (!commissioned_get()) {
        ESP_LOGI(TAG, "Configured by the coordinator");
        commissioned_set(true);
    }

    status_led_set_state(STATUS_LED_COMMISSIONED);
}
