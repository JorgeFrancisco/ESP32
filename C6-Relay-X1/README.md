| Supported Targets | ESP32-C6 |
| ----------------- | -------- |

# ESP32-C6-Relay_X1 Matter over Thread Relay

Matter **On/Off Plug-in Unit** over Thread for the **ESP32-C6-Relay_X1** single
relay board, with a factory reset on the BOOT button and a status LED.
Commissioned over Bluetooth LE and controlled through a Thread Border Router
(for example a Tuya/MOES "Matter & Thread" gateway).

Port of [C6-Thread](../C6-Thread) (itself a port of [H2-Thread](../H2-Thread))
to this board: same Matter firmware and features; the pins, the status LED
(single color instead of WS2812), the 4 MB flash layout and the factory data
change (see "Differences from C6-Thread"). The ESP32-C6 Wi-Fi radio is not
used; Thread runs on its IEEE 802.15.4 radio.

Built with ESP-IDF v5.5.5 and `espressif/esp_matter` 1.6 from the ESP Component
Registry (no esp-matter clone or `ESP_MATTER_PATH` needed).

## Features

- On/Off cluster drives a relay; the state is persisted by Matter.
- `StartUpOnOff` (Lighting feature): relay state at boot. Default: previous state.
- Commissioning over Bluetooth LE, then Thread (Full Thread Device, can act as a
  Thread router).
- Per-board factory data: own QR code, device attestation certificate and
  vendor/product names (generated with `esp-matter-mfg-tool`).
- Hold **BOOT for 10 s** to factory reset. Removing the device from its last
  controller also factory resets it, so it can be commissioned again.
- Status LED showing the commissioning/network state with blink patterns; the
  relay has its own indicator LED on the board.
- Read-only REST API over Thread (`GET /api/config`): relay state and power-on
  behavior as JSON.

## Hardware

Board: **ESP32-C6-Relay_X1** single relay development board (57.5 x 53.7 mm),
with an **ESP32-C6-WROOM-1** module (ESP32-C6 QFN40 revision v0.2, **4 MB**
flash), a 5 V **SRD-05VDC-SL-C** relay (NO/COM/NC, up to 10 A 250 VAC /
30 VDC) with an indicator LED, a USB Type-C port and a 7-60 VDC input.

| Function   | GPIO | Notes                                              |
| ---------- | ---- | -------------------------------------------------- |
| Status LED | 2    | On-board red LED, active high                      |
| BOOT       | 9    | On-board button, active low (also the boot key)    |
| Relay      | 19   | Active high (`RELAY_ACTIVE_LEVEL`)                 |

Pins and timings are in `main/app_config.h`.

Power: **either** the USB Type-C port (5 V) **or** the 7-60 VDC terminal, not
both. The Type-C port is the native USB of the ESP32-C6 (USB Serial/JTAG,
VID `303A` PID `1001`): `idf.py flash` and `monitor` work through it without a
separate USB-UART adapter. The `JP1` header (5V, 3V3, TX, RX, GND) is the UART0
burn interface for an external adapter.

The relay contacts carry mains voltage when the load is mains powered: wire it
with the board disconnected and keep it in an insulated enclosure.

## Status LED

The relay indicator LED (next to the relay) shows the relay output. The
programmable LED (GPIO2) shows the commissioning and network state, with
patterns repeated every second:

| LED                          | Meaning                                                   |
| ---------------------------- | --------------------------------------------------------- |
| Off                          | Not commissioned, commissioning window closed             |
| Slow blink (0.5 s on/off)    | Waiting to be commissioned (Bluetooth LE advertising)     |
| Fast blink (5 times/s)       | Commissioning in progress                                 |
| On                           | Commissioned, joining the Thread network                  |
| On, then off after 10 s      | Attached to the Thread network                            |
| Double flash (for 3 s)       | Commissioning failed, removed, or factory reset           |
| Short flash every second     | Commissioned but no Thread network for 3 minutes          |

After 10 s connected (`STATUS_LED_CONNECTED_MS`) the LED turns off. Any other
state (commissioning, removal, errors, Thread network lost) shows the status
again; after reconnecting, the LED is on for 10 s once more.

When the commissioning window closes without being commissioned (LED off), hold
BOOT for 10 s to open it again.

## Requirements

- A **Thread Border Router** that supports Matter commissioning. A Zigbee-only
  hub does not work: the device talks Thread, not Zigbee.
- A Matter controller app (Tuya/Smart Life, Google Home, Apple Home, Home
  Assistant, ...). Bluetooth must be enabled on the phone.
- Factory data for the board (see "Factory data"). The firmware reads the
  commissioning data from the `fctry` partition; without it the device cannot
  be commissioned.
- Internet access on the first build (the Component Manager downloads
  `esp_matter`).

## Build and flash

### Windows (PowerShell)

With ESP-IDF v5.5.5 installed by the ESP-IDF Installation Manager (EIM) in
`C:\Espressif`. Adjust the paths and the serial port (`COM6`) to your setup.

```powershell
# 1. Allow PowerShell to run scripts (only once per Windows user)
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned

# 2. Load the ESP-IDF environment (once per terminal window)
. 'C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1'

# 3. Short Component Manager cache (see "Windows path length" below).
#    Once per terminal window, or permanently with:
#    [Environment]::SetEnvironmentVariable('IDF_COMPONENT_CACHE_PATH', 'C:\Espressif\cc', 'User')
$env:IDF_COMPONENT_CACHE_PATH = 'C:\Espressif\cc'

# 4. Clone the repository (only once) and go to the project folder.
#    Keep the clone path short (see "Windows path length" below).
git clone https://github.com/JorgeFrancisco/ESP32.git
cd ESP32\C6-Relay-X1

# 5. Build (the target esp32c6 comes from sdkconfig.defaults).
#    The first build downloads esp_matter and takes a while.
idf.py build

# 6. Erase the flash on the first flash (and to forget fabrics/Thread network).
#    This also erases the factory data: flash it again in step 8.
idf.py -p COM6 erase-flash

# 7. Flash the firmware
idf.py -p COM6 flash

# 8. Flash the factory data of this board (see "Factory data")
python -m esptool --chip esp32c6 -p COM6 write_flash 0x3E0000 <uuid>-partition.bin

# 9. Open the serial monitor (exit with Ctrl+])
idf.py -p COM6 monitor
```

In VS Code with the ESP-IDF extension, **"ESP-IDF: Open ESP-IDF Terminal"**
opens a terminal with the environment already loaded (steps 1 and 2 are not
needed; step 3 is, unless the variable was set permanently).

After adding or removing source files in `main/`, run `idf.py reconfigure`
before building: `SRC_DIRS` only collects the sources when CMake configures.
After changing `sdkconfig.defaults`, delete `sdkconfig` so the defaults are
applied again.

### Windows path length

The `esp_matter` package contains the Matter SDK sources with very long file
names. With Windows long paths disabled (the default), paths are limited to 260
characters:

- The default Component Manager cache (`%LOCALAPPDATA%\Espressif\ComponentManager\Cache`)
  exceeds the limit and the download fails with `FileNotFoundError` while
  extracting `esp_matter`. `IDF_COMPONENT_CACHE_PATH` moves it to a short folder.
- In `managed_components`, the longest file path below the project folder has
  217 characters, so the project folder path must have at most 42 characters
  (for example `C:\Projetos\ESP32\C6-Relay-X1` has 29), or long paths must be
  enabled.

Enabling Windows long paths removes both limits (administrator PowerShell,
then open a new terminal):

```powershell
New-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -Value 1 -PropertyType DWORD -Force
```

### Other systems

```bash
. $HOME/esp/esp-idf/export.sh     # or your ESP-IDF activation script
idf.py build
idf.py -p <PORT> erase-flash
idf.py -p <PORT> flash
python -m esptool --chip esp32c6 -p <PORT> write_flash 0x3E0000 <uuid>-partition.bin
idf.py -p <PORT> monitor
```

## Factory data (QR code, certificate and names)

Each board has its own factory data, flashed to the `fctry` partition
(`0x3E0000`, 24 KB): commissioning passcode and discriminator (the QR code),
device attestation certificate (DAC) and key, and the device information shown
by the controllers (vendor name, product name, hardware version, VID/PID,
serial number). It is generated with
[esp-matter-mfg-tool](https://github.com/espressif/esp-matter-tools/tree/main/mfg_tool),
and the QR code comes from the same generation as the data flashed to the board.

Tested on the ESP32-C6-Relay_X1: after flashing the firmware and its own
factory partition at `0x3E0000`, the board boots with the 4 MB table, reads the
factory data (vendor/product IDs and discriminator) and starts Bluetooth LE
advertising. Commissioned in Home Assistant with its QR code (Companion app,
Thread through the Home Assistant border router, next to the C6-Thread board):
the relay follows the on/off from Home Assistant.

> **Never commit the generated files.** They include the DAC private key and
> the passcode verifier of the board. Keep them in private storage: they are
> needed to flash the same identity again after an erase. `out/` is ignored by
> `.gitignore` for that reason.

### 1. Install the tool (Windows workaround)

`esp-matter-mfg-tool` 1.0.25 pins `cffi==1.15.0` for Python < 3.13, which has no
prebuilt wheel for the Python 3.11 shipped with ESP-IDF, so `pip` tries to
compile it and fails without Microsoft C++ Build Tools. Install it in a separate
virtual environment (not the ESP-IDF one, to avoid changing its packages) with
the `cffi` version the tool itself uses on Python 3.13:

```powershell
$venv = "$env:USERPROFILE\mfgvenv"
& 'C:\Espressif\tools\python\python.exe' -m venv --without-pip $venv
Invoke-WebRequest -UseBasicParsing https://bootstrap.pypa.io/get-pip.py -OutFile get-pip.py
& "$venv\Scripts\python.exe" get-pip.py
& "$venv\Scripts\python.exe" -m pip install --no-deps esp-matter-mfg-tool
& "$venv\Scripts\python.exe" -m pip install "bitarray>=2.6.0" "cryptography==44.0.1" "cffi==1.17.1" `
  "future==0.18.3" "pycparser==2.21" "pypng==0.0.21" "PyQRCode==1.2.1" "python_stdnum==1.18" `
  "esp-secure-cert-tool==2.3.6" "ecdsa==0.19.0" "esp_idf_nvs_partition_gen==0.1.9" `
  "click==8.1.7" "click-option-group==0.5.7"
& "$venv\Scripts\esp-matter-mfg-tool.exe" --help
```

`pip` reports that `esp-matter-mfg-tool` requires `cffi==1.15.0`; that warning is
expected. The Python bundled with ESP-IDF has no `ensurepip`, hence
`--without-pip` and `get-pip.py`.

### 2. Get the Matter test credentials

They are not included in the `esp_matter` component. Download them from
[connectedhomeip](https://github.com/project-chip/connectedhomeip/tree/93abd8e6891bb578ea63254fb29d099936f345c8/credentials/test),
the commit used by `esp_matter` 1.6:

- `credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Cert.pem`
- `credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Key.pem`
- `credentials/test/certification-declaration/Chip-Test-CD-FFF2-8001.der`

The SDK only ships test Certification Declarations for vendors `0xFFF2` and
`0xFFF3`, so the board uses VID `0xFFF2`, PID `0x8001`. The DAC generated from
this test PAI is still a test certificate: controllers show the device as
uncertified and Home Assistant needs `enable_test_net_dcl`.

### 3. Generate the factory data

```powershell
& "$venv\Scripts\esp-matter-mfg-tool.exe" -v 0xFFF2 -p 0x8001 `
  --vendor-name "<vendor name>" --product-name "<product name>" `
  --hw-ver 1 --hw-ver-str v1.0 `
  --pai -k Chip-Test-PAI-FFF2-8001-Key.pem -c Chip-Test-PAI-FFF2-8001-Cert.pem `
  -cd Chip-Test-CD-FFF2-8001.der `
  --passcode <8 digits> --discriminator <0-4095> `
  --outdir <private folder>
```

- `--vendor-name`, `--product-name` and `--hw-ver-str` are what controllers show
  as vendor, product and hardware version (instead of `TEST_VENDOR`,
  `TEST_PRODUCT` and `TEST_VERSION`).
- Passcode: 8 digits from `00000001` to `99999998`, except `11111111`,
  `22222222` ... `99999999`, `12345678` and `87654321`.
- Discriminator: `0` to `4095`; use a different one for each board.
- Always pass `--outdir`: the default is the current directory.

Output in `<outdir>/fff2_8001/<uuid>/`:

| File                    | Content                                                    |
| ----------------------- | ---------------------------------------------------------- |
| `<uuid>-partition.bin`  | Factory partition to flash (24,576 bytes)                  |
| `<uuid>-qrcode.png`     | QR code                                                    |
| `<uuid>-onb_codes.csv`  | QR payload (`MT:...`), manual code, discriminator, passcode |
| `internal/`             | DAC certificate and **private key**, PAI certificate       |

The QR code can also be shown from its payload at
[project-chip.github.io/connectedhomeip/qrcode.html](https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:Y.K9042C00KA0648G00)
by replacing the `data=` value with the board's `MT:` payload.

### 4. Firmware configuration

`sdkconfig.defaults` already selects the factory data providers:

```
CONFIG_ENABLE_TEST_SETUP_PARAMS=n
CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER=y
CONFIG_ENABLE_ESP32_DEVICE_INSTANCE_INFO_PROVIDER=y
CONFIG_FACTORY_PARTITION_DAC_PROVIDER=y
CONFIG_FACTORY_COMMISSIONABLE_DATA_PROVIDER=y
CONFIG_FACTORY_DEVICE_INSTANCE_INFO_PROVIDER=y
CONFIG_CHIP_FACTORY_NAMESPACE_PARTITION_LABEL="fctry"
```

The label must match the `fctry` partition of `partitions.csv` (the Matter SDK
default is `nvs`).

### 5. Flash and check

Flash the firmware and `<uuid>-partition.bin` at `0x3E0000` (steps 6 to 8 of
"Build and flash"). At boot the log shows the values read from the factory
partition:

```
chip[DIS]: Advertise commission parameter vendorID=65522 productID=32769 discriminator=<discriminator>/..
```

Then commission with the board's QR code or manual code.

### Going back to the Matter test codes

Replace the lines of step 4 in `sdkconfig.defaults` with
`CONFIG_ENABLE_TEST_SETUP_PARAMS=y`, delete `sdkconfig` and rebuild. The board
then uses the standard test data (QR code `MT:Y.K9042C00KA0648G00`, manual code
`34970112332`, VID `0xFFF1`, PID `0x8000`) and no factory partition is needed.

## Commissioning

1. Power the board: the red status LED blinks slowly.
2. In the app, add a **Matter** device and scan the board's QR code (or type
   its manual code). Choose the home where the Thread Border Router is.
3. Tuya-based apps (Tuya/Smart Life, Nova Digital) may ask for the Wi-Fi SSID
   and password: skip that step and tell the app the device does not use Wi-Fi
   (the ESP32-C6 has Wi-Fi, but this firmware only uses Thread).
4. With test certificates the app may warn that the device is **not
   certified**; if it offers to continue, accept. Apps that refuse uncertified
   devices cannot commission this firmware.
5. The status LED blinks fast while commissioning, then stays on for 10 s once
   the device is attached to the Thread network and turns off. The device shows up as a plug/outlet and
   the relay follows the app's on/off. The first commands right after adding it
   may not reach the device while it is still settling in the Thread network.

Deleting the device in the app removes its fabric; with no fabric left the
device factory resets and blinks slowly again. The factory data (QR code,
certificate, names) is not affected by a factory reset, only by `erase-flash`.

### Home Assistant first, then Tuya (multi-admin)

A Matter device can be controlled by several controllers at the same time, each
with its own fabric. To use Home Assistant as the main controller and still see
the device in the Tuya app:

1. In the Home Assistant **Matter Server** app configuration, enable
   `enable_test_net_dcl` and restart it. Without it Home Assistant rejects the
   test certificates and aborts right after device attestation.
2. Make sure the phone knows the Thread network credentials (Home Assistant
   Thread integration, preferred network synced to the Companion app).
3. Factory reset the board (hold BOOT for 10 s) and commission it from the
   Home Assistant Companion app: **Settings > Connectivity > Matter > Add
   device > "No, it's new"**, then scan the board's QR code. It can take a few
   minutes: the phone commissions the device first and then hands it over to
   Home Assistant (two fabrics appear in the log).
4. In Home Assistant, open the device and tap **Share device**: it opens a
   commissioning window and shows a temporary sharing QR code (and pairing
   code).
5. In the Tuya app, open the **gateway that has Thread**, then **Sub-device**
   (*Sub dispositivo*) > **Device management** (*Gerenciamento de
   dispositivos*) > **Scan the code to add** (*Escaneie o código para
   adicionar*), and point it at the sharing QR code shown by Home Assistant
   (not the board's QR code). The sharing window is temporary: if it expires,
   tap **Share device** again.
6. When the app warns that the device is **not certified**, choose **Add
   anyway** (*Adicionar mesmo assim*).

Removing the device from one controller only removes that controller's fabric;
the device factory resets only when the last fabric is removed. See
"Home Assistant and Tuya hubs" below before changing the power-on behavior.

On Android, the Companion app commissions through Google Play Services, which
keeps its own fabric (vendor ID `0x6006`, shown at boot as `Fabric index ...
VendorId 0x6006`). Removing the device from Home Assistant then leaves that
fabric behind and the device does not go back to commissioning mode: also
remove it on the phone, in **Settings > Google services > All services > Matter
devices**: switch to **This Android device**, tap the device and choose
**Remove device** (in Portuguese: *Configurações > Serviços do Google > Todos os
serviços > Dispositivos Matter > Este dispositivo Android > Remover
dispositivo*). Or hold BOOT for 10 s.

## REST API

A read-only HTTP server (`esp_http_server`, port 80) starts when the device
attaches to the Thread network:

```bash
curl -g "http://[<device IPv6 address>]/api/config"
```

```json
{"relay":{"on":false},"power_on_behavior":{"start_up_on_off":0,"mode":"off"}}
```

| Field                               | Meaning                                         |
| ----------------------------------- | ----------------------------------------------- |
| `relay.on`                          | Current relay state (OnOff attribute)           |
| `power_on_behavior.start_up_on_off` | `StartUpOnOff` value: `0`, `1`, `2` or `null`   |
| `power_on_behavior.mode`            | `off`, `on`, `toggle` or `previous` (`null`)    |

There is no authentication, so the endpoint is read-only on purpose; control
stays on Matter.

### Finding the address

Thread is IPv6 only: the device has no IPv4 address and does not show up in
the router's client list. 10 s after attaching (`REST_API_ADDRESS_LOG_DELAY_MS`)
the addresses are logged, for example:

```
rest_api: IPv6 (link-local) fe80:0000:0000:0000:b0d0:9ef9:2d5d:122c
rest_api: IPv6              fd1a:9c98:026f:99fb:2676:ef0c:9799:2261   <- mesh-local (Thread only)
rest_api: IPv6              fdeb:6781:6cda:0001:d5e3:c59b:d9f7:1ab3   <- reachable from the LAN
```

Use the address in the prefix the border router announces on the LAN, the one
the computer has a route to (on Windows: `Get-NetRoute -AddressFamily IPv6`).
Tested from a Windows PC on Wi-Fi through the Tuya hub, without any manual
route: HTTP 200 in about 0.2 s.

### Resource usage

Measured on this build (`idf.py size`, `size-components`, `size-files` and the
free heap logged when the server starts):

| Item                                                            | Cost                                            |
| --------------------------------------------------------------- | ----------------------------------------------- |
| Flash: `http_parser` + `esp_http_server` + `rest_api.cpp`       | 23.9 KB (12.3 + 10.4 + 1.1)                     |
| Flash: whole application image                                  | +42.8 KB (1,702,048 → 1,744,832 bytes)          |
| Static RAM (`.bss`/`.data`)                                     | negligible (6 bytes in `rest_api.cpp`)          |
| Heap when the server starts (task stack, control socket, state) | ~7.2 KB (57,452 → 50,288 bytes free)            |

The flash figures were measured when the server was added, before the NimBLE
and libc reductions of "Memory footprint" below; the heap figure is from the
current build. The rest of the image growth is other code the server pulls in
(most likely the lwIP TCP/socket API, which Matter alone does not use). The
server is configured for 1 URI handler and 2 simultaneous connections to keep
the heap cost low: after Matter starts only about 50 KB of heap is left, so keep
additions small.

## Memory footprint

Measured with ESP-IDF v5.5.5, esp-matter 1.6 and the `sdkconfig.defaults` of
this project (`idf.py size`). The firmware is the same as C6-Thread, so the
heap figures of the [C6-Thread README](../C6-Thread) apply (about 248 KB free
after Matter and the REST server start, on the same ESP32-C6 chip).

| Item                                    | Size                                       |
| --------------------------------------- | ------------------------------------------ |
| Application image (`.bin`)              | 1,604,992 bytes (`0x187D80`)               |
| OTA slot (`ota_0`/`ota_1`, each)        | 1,966,080 bytes (`0x1E0000`)               |
| Free in the OTA slot                    | 361,088 bytes (18 %)                       |
| Code (`.text`) + constants (`.rodata`)  | 1,324,606 + 183,188 bytes                  |
| DIRAM (HP SRAM), static                 | 179,450 of 452,112 B (39.7 %)              |
| LP SRAM                                 | 24 of 16,384 B                             |

The 4 MB flash still holds two OTA slots: the image is the same size as the
ESP32-H2 one, which uses the same layout. Keep the 18 % margin in mind before
adding features; if the image outgrows the slot, the only way out is dropping
OTA (one 3.8 MB app partition) or trimming the firmware.

## Partition table (4 MB)

`partitions.csv` is the 4 MB layout of the ESP32-H2 version, with explicit
offsets. `CONFIG_PARTITION_TABLE_OFFSET` is `0xC000`.

| Partition            | Type / subtype       | Offset      | Size                 | Use                                                     |
| -------------------- | -------------------- | ----------- | -------------------- | ------------------------------------------------------- |
| (bootloader)         |                      | `0x0`       | 48 KB                | Second stage bootloader                                 |
| (partition table)    |                      | `0xC000`    | 4 KB                 |                                                         |
| `esp_secure_cert`    | `0x3F`               | `0xD000`    | 8 KB                 | Kept from the esp-matter layout (DAC comes from `fctry`) |
| `nvs`                | data / nvs           | `0x10000`   | 48 KB (12 pages)     | Matter fabrics and KVS, OpenThread dataset, attributes  |
| `nvs_keys`           | data / nvs_keys      | `0x1C000`   | 4 KB                 | Only used with NVS encryption (disabled)                |
| `otadata`            | data / ota           | `0x1D000`   | 8 KB                 | Selects the OTA slot to boot                            |
| `phy_init`           | data / phy           | `0x1F000`   | 4 KB                 | Kept for compatibility (PHY data is built in)           |
| `ota_0`              | app / ota_0          | `0x20000`   | 1,920 KB             | Firmware, slot A                                        |
| `ota_1`              | app / ota_1          | `0x200000`  | 1,920 KB             | Firmware, slot B                                        |
| `fctry`              | data / nvs           | `0x3E0000`  | 24 KB                | Factory data (QR code, DAC, device info)                |

- **NVS:** 48 KB was enough on the ESP32-C6 DevKitC with two fabrics (85
  written entries of 1,296, checked with `nvs_tool.py`), and
  `CONFIG_MAX_FABRICS` is 5.
- The last 104 KB (`0x3E6000`-`0x400000`) are unallocated.
- **Changing the table requires `erase-flash`:** it erases the fabrics, the
  Thread network and the factory data. Flash the firmware, write the factory
  partition at `0x3E0000` and commission the device again (remove the old node
  from the controllers first).

## Differences from C6-Thread

| Item              | C6-Relay-X1 (this project)                                  | C6-Thread                                       |
| ----------------- | ----------------------------------------------------------- | ----------------------------------------------- |
| Board             | ESP32-C6-Relay_X1 (ESP32-C6-WROOM-1), relay on the board    | ESP32-C6 N16 DevKitC, external relay module     |
| Flash             | 4 MB (`CONFIG_ESPTOOLPY_FLASHSIZE_4MB`)                     | 16 MB, Boya chip driver enabled                 |
| Partition table   | 4 MB: 1.875 MB OTA slots, 48 KB NVS, `fctry` at `0x3E0000`  | 16 MB: 4 MB OTA slots, 92 KB NVS, `fctry` at `0x2A000`, LittleFS `storage`, `coredump` |
| Relay             | GPIO19, active high                                         | GPIO10, active high                             |
| Status LED        | GPIO2, single color, blink patterns; relay has its own LED  | GPIO8, WS2812 RGB, colors, mirrors the relay    |
| BOOT              | GPIO9                                                       | GPIO9                                           |
| Factory data      | Own QR code, passcode, discriminator and DAC (product name "ESP32-C6 Relay X1") | "ESP32-C6 Relay"       |
| Matter firmware   | Same (`app_main.cpp`, `relay.c`, `rest_api.cpp`, `reset_button.c`) | Same                                     |

## Implementation notes

### Why Matter over Thread (and not Zigbee) for Tuya-based hubs

Tuya gateways only accept Zigbee sub-devices that carry a Tuya license, written
at manufacturing time to chips running the Tuya Zigbee SDK. A custom ESP32-H2
Zigbee firmware is reported as **not authorized** in the app. Tuya apps do
commission third-party Matter devices, so with a hub that includes a Thread
Border Router the ESP32-C6 can join as a standard Matter device instead.

### Home Assistant and Tuya hubs

Observed with the ESP32-H2 version, which runs the same firmware.

- Every applied attribute change is logged, including writes from controllers:
  `esp_matter_attribute: ********** W : Endpoint 0x0001's Cluster 0x00000006's Attribute 0x00004003 is 1 **********`
  (`0x4003` is `StartUpOnOff`: 0 off, 1 on, 2 toggle, null previous state).
- **Change the power-on behavior only from Home Assistant commissioned directly.**
  When the setting goes through a Tuya hub (for example Home Assistant
  controlling the device via the hub), the hub writes `StartUpOnOff` as `null`
  whatever is selected, so the device always restores its previous state and
  any value set before is overwritten. On/off commands are forwarded correctly.
- With Home Assistant commissioned directly (see "Home Assistant first, then
  Tuya"), the real value is written (`... Attribute 0x00004003 is 0` from the
  Home Assistant fabric). With the Tuya app added afterwards through sharing,
  the hub did not rewrite `StartUpOnOff` by itself during our tests.
- **Two devices in Home Assistant.** If the Tuya hub is itself in the Home
  Assistant Matter integration, it is a Matter bridge: once the board is added
  to the hub as a sub-device, the hub exposes it as one of its own endpoints
  (device types `0x010A` On/Off Plug-in Unit and `0x0013` Bridged Node) and
  Home Assistant shows a second device, "connected via" the hub. That copy
  only has the on/off switch and the power-on behavior select, while the
  device commissioned directly has all the entities (identify, firmware
  update, Thread diagnostics...). On/off is forwarded to the board, but the
  bridged `StartUpOnOff` belongs to the hub endpoint and is not read from the
  board: in our tests the board had `1` (on) and the bridged endpoint showed
  `0` (off). Changing it there writes `null` to the board (see above). Use the
  direct device and disable the bridged one in Home Assistant.
- Direct commissioning requires `enable_test_net_dcl` in the Matter Server app
  configuration (otherwise Home Assistant aborts right after device attestation,
  because of the test certificates) and the Thread credentials on the phone.

### Test certificates

The DAC in the factory data is signed by the Matter SDK **test** PAI
(`Chip-Test-PAI-FFF2-8001`) and uses test VID/PID. It is fine for development,
but controllers show the device as uncertified and some may refuse it. Real
products need a PAI from a Matter certificate authority, their own VID/PID and
Matter certification.

### Data model

The device is created with the esp-matter data model (`on_off_plug_in_unit`):
endpoint 0 is the Root Node and endpoint 1 the relay (Identify, Groups, Scenes
Management, On/Off with the Lighting feature). Unused clusters are disabled in
`sdkconfig.defaults` to save flash and RAM.

## Project structure

| File                   | Responsibility                                                          |
| ---------------------- | ----------------------------------------------------------------------- |
| `main/app_main.cpp`    | Matter node, commissioning events, status LED state                     |
| `main/app_config.h`    | Configuration (pins, timings)                                           |
| `main/relay.c`         | Relay output                                                            |
| `main/rest_api.cpp`    | Read-only REST API over Thread (IPv6)                                   |
| `main/reset_button.c`  | BOOT button (factory reset)                                             |
| `main/status_led.c`    | Status LED (GPIO, blink patterns)                                       |
| `sdkconfig.defaults`   | Target, Bluetooth LE, OpenThread, Matter, factory data providers, clusters |
| `partitions.csv`       | 4 MB layout: two 1.875 MB OTA slots, NVS, `fctry` factory data          |

## Known warnings

- `the choice symbol SEC_CERT_DAC_PROVIDER ... is defined with a prompt outside the choice`
  (and `Symbol SEC_CERT_DAC_PROVIDER defined in multiple locations`): the symbol is
  declared both in the `esp_matter` component and in the Matter SDK Kconfig
  shipped inside it. Harmless.
- The first build compiles about 2000 files; later builds are incremental. The
  application image is about 1.6 MB, 18% of the 1.875 MB OTA slots is left.
- `chip[DIS]: Failed to remove advertised services: 3`, `Failed to advertise
  commissionable node: 3` and `Failed to finalize service update: 3` at boot
  before commissioning: there is no Thread network yet to publish the DNS-SD
  (SRP) service on. Commissioning goes over Bluetooth LE; harmless.
- `ble_gap_terminate() failed: 48` / `Error CHIP:0x00000048 ... BLEManagerImpl.cpp`
  right after the device joins Thread during commissioning: the phone had
  already closed the BLE connection. Harmless.
