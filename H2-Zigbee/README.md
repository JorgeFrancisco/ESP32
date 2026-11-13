| Supported Targets | ESP32-H2 |
| ----------------- | -------- |

# ESP32-H2 Zigbee Relay

Zigbee 3.0 **On/Off Light (router)** for the ESP32-H2 that drives a relay, with
professional-style pairing (BOOT button), an RGB status LED and
`power_on_behavior` support. Tested with **Zigbee2MQTT**.

Built with ESP-IDF v5.5.5 and `espressif/esp-zigbee-lib` 2.0.4.

## Features

- On/Off cluster drives a relay; the last state is stored in NVS.
- `StartUpOnOff` (0x4003): relay state at boot (`power_on_behavior` in Zigbee2MQTT).
- Pairing only on demand: hold **BOOT for 10 s**, or automatically after being
  removed by the coordinator. The pairing window lasts 3 minutes.
- Scans all 2.4 GHz channels (11-26) and joins the network that permits joining.
- WS2812 status LED showing the network/commissioning state.

## Hardware

Board: **ESP32-H2-DevKitM-1** (ESP32-H2-MINI-1).

| Function   | GPIO | Notes                                  |
| ---------- | ---- | -------------------------------------- |
| Status LED | 8    | On-board addressable RGB LED (WS2812)  |
| BOOT       | 9    | On-board button, active low            |
| Relay      | 10   | Active high (`RELAY_ACTIVE_LEVEL`)     |

Pins, timings and the device identity are in `main/all_device_types_app.h`.

## Status LED

| LED               | Meaning                                              |
| ----------------- | ---------------------------------------------------- |
| Off               | No network, waiting for the BOOT button              |
| Blue, blinking    | Pairing mode (searching for a network)               |
| Purple, blinking  | Joined, coordinator interview in progress            |
| Purple, solid     | Interview complete                                   |
| Green, solid      | Configured/used by the coordinator                   |
| Red, solid (3 s)  | Removed from the network (or failed to join)         |
| Red, blinking     | Joined for 3 minutes without completing the interview |

## Build and flash

### Windows (PowerShell)

With ESP-IDF v5.5.5 installed by the ESP-IDF Installation Manager (EIM) in
`C:\Espressif`. Adjust the paths and the serial port (`COM4`) to your setup.

```powershell
# 1. Allow PowerShell to run scripts (only once per Windows user)
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned

# 2. Load the ESP-IDF environment (once per terminal window).
#    The leading dot runs the script in the current terminal, so the
#    environment variables (IDF_PATH, PATH) remain available.
. 'C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1'

# 3. Go to the project folder
cd C:\Users\jorge\WORKSPACE\ESP32\H2-Zigbee

# 4. Only the first time (there is no build/ folder yet)
idf.py set-target esp32h2

# 5. Build
idf.py build

# 6. Optional: erase the flash (forgets the network, relay state and power_on_behavior)
idf.py -p COM4 erase-flash

# 7. Flash and open the serial monitor (exit with Ctrl+])
idf.py -p COM4 flash monitor
```

In VS Code with the ESP-IDF extension, **"ESP-IDF: Open ESP-IDF Terminal"**
opens a terminal with the environment already loaded (steps 1 and 2 are not
needed).

### Other systems

```bash
. $HOME/esp/esp-idf/export.sh     # or your ESP-IDF activation script
idf.py set-target esp32h2
idf.py build
idf.py -p <PORT> erase-flash      # optional: forget the stored network and state
idf.py -p <PORT> flash monitor
```

## Pairing with Zigbee2MQTT

1. Enable **Permit join (All)** in Zigbee2MQTT.
2. Hold **BOOT for 10 s**: the LED blinks blue.
3. The device scans, selects the open network and joins. The LED turns purple,
   then green once Zigbee2MQTT configures the On/Off cluster.
4. The device shows up as `ESPRESSIF` `ESP32H2` (generated definition) and the
   `state` toggles the relay.

Removing the device in Zigbee2MQTT (without *force remove*) makes it leave the
network and enter pairing mode again. With *force remove*, hold BOOT for 10 s.

## Project structure

| File                          | Responsibility                                                  |
| ----------------------------- | --------------------------------------------------------------- |
| `main/all_device_types_app.c` | `app_main`, Zigbee task, endpoint creation, stack signals        |
| `main/all_device_types_app.h` | Configuration (pins, identity, timings, network)                 |
| `main/network.c`              | BOOT button, pairing window, network scan/selection, join/leave  |
| `main/zb_responses.c`         | ZDO/ZCL responses sent by the application                        |
| `main/relay.c`                | Relay output and `StartUpOnOff`                                  |
| `main/status_led.c`           | WS2812 status LED                                                |
| `main/app_storage.c`          | Small NVS key/value helpers                                      |

## Implementation notes

These were found while pairing the device with different coordinators.

### Tuya-based hubs (e.g. Nova Digital) do not accept this device

Tuya gateways complete the Zigbee interview, but the app then reports the
device as **not authorized**: Tuya requires a license written to the chip at
manufacturing time, which a custom ESP32-H2 firmware does not have. Imitating
Tuya identities (manufacturer name, model, clusters) does not work around it.
Use an open coordinator such as Zigbee2MQTT instead.

### Responses sent by the application

With the Nova Digital coordinator, the responses generated by the stack for
the interview requests never reached the coordinator (it kept repeating the
requests). `zb_responses.c` answers them from the application, in standard
Zigbee format:

- ZDO: `NWK_Addr_req`, `IEEE_Addr_req`, `Active_EP_req`, `Simple_Desc_req`,
  `Match_Desc_req`
- ZCL: Basic cluster *Read Attributes*

The Simple Descriptor sent there must match the clusters of the created
endpoint (`s_server_clusters`).

### No APS ACK before the join completes

Requesting APS acknowledgements while the Trust Center link key exchange was
still running caused retransmissions and the join failed with BDB status
`0x0a`. Acknowledgements are only requested after network steering succeeds.

### Crowded channel: select the open network explicitly

With another network of ~25 routers on the same channel, network steering often
failed with "no network" (BDB status `0x03`) even though Zigbee2MQTT was
permitting joins. The device now runs an active scan first, picks the network
that permits joining and pins its extended PAN ID
(`ezb_set_use_extended_panid`) before steering. The pin is cleared after
joining.

## Known warnings

- `spi_flash: Detected size(4096k) larger than the size in the binary image header(2048k)`:
  the module has 4 MB of flash and the project is configured for 2 MB. Harmless;
  change the flash size in `menuconfig` if more space is needed.
- `rmt: channel resolution loss`: RMT clock rounding for the LED timing. Harmless.
