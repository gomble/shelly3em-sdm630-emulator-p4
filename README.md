# Shelly 3EM → SDM630 Modbus Emulator (ESP32-P4 / Ethernet / ESP-IDF)

Makes a [Shelly 3EM](https://www.shelly.com/) power meter appear as a genuine
**Eastron SDM630 Modbus RTU energy meter** to a hybrid solar inverter — so you
can use a cheap Wi-Fi power meter where the inverter insists on a "real"
RS485 Modbus meter.

Built and tested against a **Growatt MIN TL-XH** inverter, but the Modbus
register layout follows the standard SDM630 convention, so it should work
with any inverter/charger that expects an SDM630-compatible meter on its
Modbus input.

This is an ESP32-P4 (wired Ethernet, ESP-IDF) rewrite of an earlier ESP32
(Wi-Fi, Arduino) version of the same idea, done to get rid of recurring
Wi-Fi-related instability. See [`docs/HARDWARE.md`](docs/HARDWARE.md) for why.

## How it works

```
Shelly 3EM  --HTTP (polled every 1s)-->  ESP32-P4  --Modbus RTU / RS485-->  Inverter
```

The ESP32-P4:
1. Polls the Shelly 3EM's local HTTP API (`/status`) once per second over
   Ethernet.
2. Stores the live voltage/current/power/energy values in an internal
   register table.
3. Answers Modbus RTU "Read Input/Holding Registers" requests from the
   inverter with those values, using the same register layout as a real
   Eastron SDM630.

## Why this exists / background

The inverter only accepts a genuine Modbus RTU meter on its smart-meter
input — it has no concept of a Shelly device. Rather than buying a real
SDM630 alongside a Shelly you already own, this project makes the ESP32
"speak Modbus" on the inverter's behalf.

The Modbus slave logic in `esphome/components/p4_modbus_probe` is a
from-scratch, dependency-free reimplementation (CRC16 validation, function
codes 0x03/0x04, configurable slave address and register table) written
specifically because the ESP32-P4 requires ESP-IDF, and the original
Arduino-only `modbus_server` external component this idea was originally
built on does not compile under ESP-IDF.

## Quick start

1. **Hardware**: see [`docs/HARDWARE.md`](docs/HARDWARE.md) for the exact
   board used and important ESP32-P4-specific caveats (ESP-IDF required,
   `engineering_sample` flag, Ethernet pin table).
2. **Wiring**: see [`docs/WIRING.md`](docs/WIRING.md) for the RS485/UART
   wiring between the ESP32-P4 and your inverter's Modbus bus.
3. **Configure**:
   ```bash
   cd esphome
   cp secrets.yaml.example secrets.yaml
   # edit secrets.yaml: generate your own api_encryption_key, set your
   # network_domain (".local" or your router's domain, e.g. ".fritz.box")
   ```
   Then edit the `substitutions:` block at the top of
   `shelly3em-sdm630-emulator-p4.yaml`:
   - `shelly_ip` — the IP address of your real Shelly 3EM
   - `modbus_slave_id` — the Modbus address your inverter expects for its
     meter (see [`docs/MODBUS-NOTES.md`](docs/MODBUS-NOTES.md) on how to
     verify this for your own installation before assuming the default)
4. **Compile & flash** (first time, over USB):
   ```bash
   esphome run shelly3em-sdm630-emulator-p4.yaml
   ```
   Subsequent updates can be done over-the-air:
   ```bash
   esphome upload shelly3em-sdm630-emulator-p4.yaml --device <device-ip>
   ```
5. **Wire it up** to the RS485 bus per `docs/WIRING.md`, power it over PoE,
   and the inverter should start seeing live meter data within a few
   seconds of the Shelly responding.

## Diagnostics exposed in Home Assistant

| Entity | Meaning |
|---|---|
| `Modbus Requests An Uns` | Count of valid Modbus requests addressed to this device (updates every 60s, not per-request, to avoid flooding your recorder database) |
| `Modbus Antworten Gesendet` | Count of responses sent — should always equal the request count |
| `Modbus CRC Errors` | Count of corrupted frames received for our address (published immediately, since these should stay at 0) |
| `Modbus Status` | Human-readable status text ("Daten aktualisiert", IP changes, etc.) |
| Live meter sensors | Voltage/current/power per phase, frequency, total energy — same values the Shelly reports |

## Repo layout

```
esphome/
  shelly3em-sdm630-emulator-p4.yaml   # main ESPHome config (sanitized, no personal secrets)
  secrets.yaml.example                # copy to secrets.yaml and fill in your own values
  components/
    p4_modbus_probe/                  # custom ESP-IDF-compatible Modbus RTU slave component
docs/
  HARDWARE.md                         # board used, pinout, ESP32-P4-specific caveats
  WIRING.md                           # RS485/UART wiring diagram
  MODBUS-NOTES.md                     # register map + how to verify your own inverter's expectations
```

## Known limitations / things to check for your own setup

- Only Modbus function codes `0x03` and `0x04` (read) are implemented — no
  write support, since a meter emulator doesn't need it.
- Assumes an **auto-direction** RS485 transceiver module (no DE/RE GPIO
  control). If yours needs explicit direction switching, you'll need to add
  that to the component.
- The Ethernet pinout in the main YAML is specific to the board this was
  built on (LUCKFOX ESP32-P4-POE-ETH-NH) — check your own board's schematic
  if you're using different ESP32-P4 Ethernet hardware.
- `modbus_slave_id` and the register map default to what this particular
  Growatt MIN TL-XH installation expected — verify against your own
  inverter's traffic before trusting it blindly.

## License

MIT — see [`LICENSE`](LICENSE).
