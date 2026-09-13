/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

/*
 * Answers coordinator requests from the application (standard Zigbee format,
 * valid for any coordinator) and watches the frames that mark the end of the
 * interview and the On/Off configuration.
 */
void zb_responses_register(void);
