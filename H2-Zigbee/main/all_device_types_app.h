/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_zigbee.h"

/* Device */
#define ZIGBEE_ENDPOINT                         1
#define ZIGBEE_TX_POWER_DBM                     20

/* Basic cluster identity (ZCL strings: first byte is the length) */
#define ZIGBEE_MANUFACTURER_NAME                "\x09""ESPRESSIF"
#define ZIGBEE_MODEL_IDENTIFIER                 "\x07""ESP32H2"
#define ZIGBEE_DATE_CODE                        "\x08""20260912"
#define ZIGBEE_SW_BUILD_ID                      "\x05""1.0.0"
#define ZIGBEE_APP_VERSION                      1
#define ZIGBEE_STACK_VERSION                    0
#define ZIGBEE_HW_VERSION                       1

/* Status LED: addressable RGB LED (WS2812) of the ESP32-H2-DevKitM-1 */
#define STATUS_LED_GPIO                         8
#define STATUS_LED_BRIGHTNESS                   32      /* 0-255 */
#define STATUS_LED_BLINK_MS                     300
#define STATUS_LED_ERROR_MS                     3000    /* solid red after an error */
#define STATUS_LED_INTERVIEW_TIMEOUT_MS         180000  /* joined without interview */

/* Pairing: BOOT button of the DevKitM-1 (GPIO9, active low) */
#define PAIRING_BUTTON_GPIO                     9
#define PAIRING_BUTTON_HOLD_MS                  10000   /* hold to pair from scratch */
#define PAIRING_WINDOW_MS                       180000  /* pairing mode duration */
#define PAIRING_RETRY_MS                        5000    /* delay between join attempts */

/* Relay driven by the On/Off cluster */
#define RELAY_GPIO                              10
#define RELAY_ACTIVE_LEVEL                      1       /* relay on at high level */

/* Network */
#define MAX_CHILDREN                            10
#define INSTALLCODE_POLICY_ENABLE               false
#define ESP_ZIGBEE_STORAGE_PARTITION_NAME       "nvs"

/* Scan all 2.4 GHz channels (11-26) to find any coordinator */
#define ESP_ZIGBEE_PRIMARY_CHANNEL_MASK         0x07FFF800U
#define ESP_ZIGBEE_SECONDARY_CHANNEL_MASK       0U

#define ESP_ZIGBEE_ZR_CONFIG()                                      \
    {                                                               \
        .device_type = EZB_NWK_DEVICE_TYPE_ROUTER,                  \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .zczr_config = {                                            \
            .max_children = MAX_CHILDREN,                           \
        },                                                          \
    }

#if CONFIG_SOC_IEEE802154_SUPPORTED

#define ESP_ZIGBEE_PLATFORM_CONFIG()                                \
    {                                                               \
        .storage_partition_name = ESP_ZIGBEE_STORAGE_PARTITION_NAME,\
        .radio_config = {                                           \
            .radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE,             \
        },                                                          \
    }

#else
#error "This project requires an IEEE 802.15.4 SoC, such as the ESP32-H2"
#endif

#define ESP_ZIGBEE_DEFAULT_CONFIG()                                 \
    {                                                               \
        .device_config = ESP_ZIGBEE_ZR_CONFIG(),                    \
        .platform_config = ESP_ZIGBEE_PLATFORM_CONFIG(),            \
    }
