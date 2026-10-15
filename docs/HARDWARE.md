# Hardware

## Used in this build

| Component | Model | Notes |
|---|---|---|
| MCU board | **LUCKFOX ESP32-P4-POE-ETH-NH** | ESP32-P4, PoE-powered, 100 Mbit Ethernet, 32 MB PSRAM, no soldered header. Any ESP32-P4 board with the same Ethernet PHY wiring should work — see pin table below. |
| PHY chip | IP101 (on-board) | 100 Mbit Ethernet PHY, RMII interface, external clock |
| RS485 transceiver | Generic MAX485-style auto-direction RS485-to-TTL converter module | Any 3.3V/5V RS485↔UART module works; auto-direction (no DE/RE control pin needed) was used here |
| Power source | PoE (802.3af) | Powered over the Ethernet cable; no separate 5V/3.3V supply needed |
| Power meter | Shelly 3EM | Polled over HTTP for live voltage/current/power per phase |
| Inverter | Growatt MIN TL-XH (hybrid) | Polls the ESP32 as if it were a genuine Eastron SDM630 Modbus energy meter |
| Battery | Growatt APX (HV, modular LFP) | Communicates with the inverter over a **separate SYS COM port**, not involved in this project |

## Why ESP32-P4 instead of a classic ESP32

The original version of this project ran on a plain ESP32 (WROOM-32, Arduino
framework) over Wi-Fi. That setup suffered from recurring Wi-Fi disconnects
under load (heavy HTTP polling + Modbus RTU + API all running concurrently on
a single core), which the inverter interpreted as meter failures.

The ESP32-P4 was chosen to eliminate the Wi-Fi instability entirely by using
wired Ethernet (PoE) instead, and it has significantly more RAM/CPU headroom
than a classic ESP32.

## Important hardware caveat: ESP-IDF only, no Arduino

The ESP32-P4 has **no (or very limited) Arduino core support** in ESPHome.
This project therefore uses:

```yaml
esp32:
  variant: esp32p4
  framework:
    type: esp-idf
```

This matters because the original project's Modbus slave implementation
(the `modbus_server` external component by Feierprinz) is Arduino-only and
**does not compile on ESP-IDF**. This repo ships a from-scratch, dependency-free
reimplementation instead — see `esphome/components/p4_modbus_probe/`.

## Ethernet pinout used in this specific board

```yaml
ethernet:
  type: IP101
  mdc_pin: GPIO31
  mdio_pin: GPIO52
  phy_addr: 1
  power_pin: GPIO51
  clk:
    pin: GPIO50
    mode: CLK_EXT_IN
```

These pins are specific to the **LUCKFOX ESP32-P4-POE-ETH-NH** board as
tested. Generic Waveshare ESP32-P4-ETH reference designs document different
pins for the same PHY (MDC=GPIO23, MDIO=GPIO18, PHY-Reset=GPIO5) — if you're
using a different ESP32-P4 Ethernet board, check your board's schematic
before trusting the pin numbers above.

## `engineering_sample` flag

Set `engineering_sample: false` in the `esp32:` block. If your board boots
in a tight restart loop (`CHIP_LP_WDT_RESET` every few seconds, no ESPHome
log output at all) immediately after flashing, double-check this setting —
it must match your chip's actual silicon revision (`esp32p4-ecoX` shown at
the very first boot lines over serial/USB).
