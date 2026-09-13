/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/*
 * ESP32-H2 Zigbee On/Off Light (router) with a relay output.
 *
 *   network.c       pairing mode, join/leave, commissioning status
 *   zb_responses.c  answers to coordinator requests
 *   relay.c         relay output and StartUpOnOff
 *   status_led.c    status LED
 */

#include "all_device_types_app.h"

#include "esp_log.h"
#include "ezbee/zha.h"
#include "network.h"
#include "nvs_flash.h"
#include "relay.h"
#include "status_led.h"
#include "zb_responses.h"

static const char *TAG = "app";

/* -------------------------------------------------------------------------- */
/* Device                                                                     */
/* -------------------------------------------------------------------------- */

/* The On/Off attribute drives the relay */
static void zcl_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message)
{
    const ezb_zcl_set_attr_value_message_t *msg = message;

    if (callback_id != EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID || msg == NULL) {
        return;
    }

    if (msg->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF &&
        msg->in.attribute.id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
        msg->in.attribute.data.value != NULL) {
        relay_set(*(uint8_t *)msg->in.attribute.data.value != 0);
    }
}

static esp_err_t add_basic_identity(ezb_af_ep_desc_t endpoint)
{
    static const struct {
        uint16_t id;
        const char *value;
    } attrs[] = {
        { EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ZIGBEE_MANUFACTURER_NAME },
        { EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,  ZIGBEE_MODEL_IDENTIFIER },
        { EZB_ZCL_ATTR_BASIC_DATE_CODE_ID,         ZIGBEE_DATE_CODE },
        { EZB_ZCL_ATTR_BASIC_SW_BUILD_ID_ID,       ZIGBEE_SW_BUILD_ID },
    };

    ezb_zcl_cluster_desc_t basic =
        ezb_af_endpoint_get_cluster_desc(endpoint, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);

    if (basic == NULL) {
        return ESP_FAIL;
    }

    for (size_t i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++) {
        ESP_ERROR_CHECK(ezb_zcl_basic_cluster_desc_add_attr(basic, attrs[i].id, attrs[i].value));
    }

    return ESP_OK;
}

/* StartUpOnOff (0x4003) is optional; Zigbee2MQTT writes it via "power_on_behavior" */
static void add_startup_on_off(ezb_af_ep_desc_t endpoint, uint8_t startup_on_off)
{
    static uint8_t value;

    ezb_zcl_cluster_desc_t on_off =
        ezb_af_endpoint_get_cluster_desc(endpoint, EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER);

    value = startup_on_off;

    if (on_off == NULL ||
        ezb_zcl_on_off_cluster_desc_add_attr(on_off, EZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF_ID,
                                             &value) != EZB_ERR_NONE) {
        ESP_LOGW(TAG, "StartUpOnOff attribute not available");
    }
}

static esp_err_t zigbee_create_device(void)
{
    uint8_t startup_on_off = relay_get_startup_on_off();
    bool initial_on = relay_initial_state(startup_on_off);

    ezb_zha_on_off_light_config_t light_cfg = EZB_ZHA_ON_OFF_LIGHT_CONFIG();

    light_cfg.basic_cfg.power_source = EZB_ZCL_BASIC_POWER_SOURCE_SINGLE_PHASE_MAINS;
    light_cfg.on_off_cfg.on_off = initial_on;

    ezb_af_ep_desc_t endpoint = ezb_zha_create_on_off_light(ZIGBEE_ENDPOINT, &light_cfg);

    if (endpoint == NULL || add_basic_identity(endpoint) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create the On/Off Light endpoint");
        return ESP_FAIL;
    }

    add_startup_on_off(endpoint, startup_on_off);

    ezb_af_device_desc_t device = ezb_af_create_device_desc();

    ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(device, endpoint));
    ESP_ERROR_CHECK(ezb_af_device_desc_register(device));

    ezb_zcl_core_action_handler_register(zcl_action_handler);
    zb_responses_register();

    relay_set(initial_on);

    ESP_LOGI(TAG, "%s %s ready on endpoint %d (StartUpOnOff 0x%02x)",
             ZIGBEE_MANUFACTURER_NAME + 1, ZIGBEE_MODEL_IDENTIFIER + 1,
             ZIGBEE_ENDPOINT, startup_on_off);

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/* Zigbee stack signals                                                       */
/* -------------------------------------------------------------------------- */

static void log_network_info(const char *message)
{
    ezb_extpanid_t ext_pan_id;

    ezb_nwk_get_extended_panid(&ext_pan_id);

    ESP_LOGI(TAG, "%s: PAN 0x%04hx, extended PAN 0x%016llx, channel %d, address 0x%04hx",
             message,
             ezb_nwk_get_panid(),
             ext_pan_id.u64,
             ezb_nwk_get_current_channel(),
             ezb_nwk_get_short_address());
}

static uint8_t bdb_status_of(const ezb_app_signal_t *signal)
{
    const ezb_bdb_signal_simple_params_t *params = ezb_app_signal_get_params(signal);

    return params != NULL ? params->status : 0xFF;
}

static void handle_stack_started(const ezb_app_signal_t *signal)
{
    uint8_t status = bdb_status_of(signal);

    if (status != EZB_BDB_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "Stack initialization failed (status 0x%02x)", status);
        return;
    }

    bool factory_new = ezb_bdb_is_factory_new();

    if (!factory_new) {
        log_network_info("Rejoined network");
    }

    network_on_stack_started(factory_new);
}

static void handle_steering(const ezb_app_signal_t *signal)
{
    uint8_t status = bdb_status_of(signal);

    if (status == EZB_BDB_STATUS_SUCCESS) {
        log_network_info("Joined network");
    }

    network_on_steering_done(status);
}

static void handle_leave(const ezb_app_signal_t *signal)
{
    const ezb_zdo_signal_leave_params_t *params = ezb_app_signal_get_params(signal);

    network_on_left(params != NULL && params->leave_type == EZB_ZDO_LEAVE_TYPE_RESET);
}

static bool zigbee_signal_handler(const ezb_app_signal_t *signal)
{
    switch (ezb_app_signal_get_type(signal)) {
    case EZB_ZDO_SIGNAL_SKIP_STARTUP:
        ezb_set_tx_power(ZIGBEE_TX_POWER_DBM);
        ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
        break;

    case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case EZB_BDB_SIGNAL_DEVICE_REBOOT:
        handle_stack_started(signal);
        break;

    case EZB_BDB_SIGNAL_STEERING:
        handle_steering(signal);
        break;

    case EZB_ZDO_SIGNAL_LEAVE:
        handle_leave(signal);
        break;

    case EZB_ZDO_SIGNAL_ERROR:
        ESP_LOGE(TAG, "ZDO error signal");
        break;

    default:
        break;
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/* Startup                                                                    */
/* -------------------------------------------------------------------------- */

static void zigbee_main_task(void *arg)
{
    (void)arg;

    esp_zigbee_config_t config = ESP_ZIGBEE_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(esp_zigbee_init(&config));

    /* Centralized network: the coordinator is the Trust Center */
    ezb_aps_secur_enable_distributed_security(false);

    ESP_ERROR_CHECK(ezb_bdb_set_primary_channel_set(ESP_ZIGBEE_PRIMARY_CHANNEL_MASK));
    ESP_ERROR_CHECK(ezb_bdb_set_secondary_channel_set(ESP_ZIGBEE_SECONDARY_CHANNEL_MASK));
    ESP_ERROR_CHECK(ezb_app_signal_add_handler(zigbee_signal_handler));
    ESP_ERROR_CHECK(zigbee_create_device());

    /* Keep the stored network (no factory reset at startup) */
    ESP_ERROR_CHECK(esp_zigbee_start(false));

    esp_zigbee_launch_mainloop();
    esp_zigbee_deinit();
    vTaskDelete(NULL);
}

static void nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
}

void app_main(void)
{
    nvs_init();

    relay_init();
    status_led_init();
    network_init();

    xTaskCreate(zigbee_main_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
