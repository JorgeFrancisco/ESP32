/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Network lifecycle: pairing mode (BOOT button or removal by the
 * coordinator), join/leave handling and commissioning status on the LED.
 *
 * The network_on_*() functions must be called from the Zigbee context.
 */

/* Starts the BOOT button task */
void network_init(void);

/* True after network steering succeeded (Trust Center link key exchanged) */
bool network_is_joined(void);

/* Stack initialization finished */
void network_on_stack_started(bool factory_new);

/* Network steering finished with the given BDB status */
void network_on_steering_done(uint8_t bdb_status);

/* The device left the network; reset_without_rejoin means removed or reset */
void network_on_left(bool reset_without_rejoin);

/* The coordinator received our Simple Descriptor */
void network_on_interview_complete(void);

/* The coordinator configured or used the On/Off cluster */
void network_on_coordinator_configured(void);
