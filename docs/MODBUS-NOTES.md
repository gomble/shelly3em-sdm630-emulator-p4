# Modbus notes

## What this device emulates

The Shelly 3EM only exposes its measurements over HTTP/JSON (its native
local API). Many hybrid inverters (this project was built for a Growatt
MIN TL-XH) expect a genuine Modbus RTU energy meter on their "smart meter"
RS485 input — typically an **Eastron SDM630**.

This project bridges the gap: it polls the Shelly 3EM over HTTP and
re-serves the same values as Modbus RTU input/holding registers, mimicking
an SDM630, so the inverter is happy without ever knowing a Shelly is
involved.

## Register map (matches the original SDM630-emulator project)

All values are IEEE-754 32-bit floats, split across two consecutive 16-bit
Modbus registers, **high word first** (register `N` = high word, register
`N+1` = low word) — the same layout Eastron SDM630 devices use.

| Register (hex) | Value |
|---|---|
| 0x0000–0x0001 | Voltage L1 (V) |
| 0x0002–0x0003 | Voltage L2 (V) |
| 0x0004–0x0005 | Voltage L3 (V) |
| 0x0006–0x0007 | Current L1 (A) |
| 0x0008–0x0009 | Current L2 (A) |
| 0x000A–0x000B | Current L3 (A) |
| 0x000C–0x000D | Active power L1 (W) |
| 0x000E–0x000F | Active power L2 (W) |
| 0x0010–0x0011 | Active power L3 (W) |
| 0x0046–0x0047 | Frequency (Hz) |
| 0x0156–0x0157 | Total energy import (kWh) |

Function codes `0x03` (Read Holding Registers) and `0x04` (Read Input
Registers) are both answered identically.

## How to find out what Modbus address/registers *your* inverter expects

Before relying on the default `modbus_slave_id: "2"`, verify it against your
own installation. The easiest way is to temporarily use the
`p4_modbus_probe` component in pure passive-logging mode:

1. Wire the ESP32-P4 to your RS485 bus (see `docs/WIRING.md`).
2. Flash a minimal config with just `uart:` and `p4_modbus_probe:`
   (`slave_address` doesn't matter for logging — every frame on the bus is
   logged at `ESP_LOGD` level regardless of which address it targets, as
   long as you temporarily set it to log unconditionally; see the component
   source for the one-line tweak, or just watch the logs while your own
   device's address happens to match).
3. Watch the live logs (`esphome logs your-config.yaml --device <ip>`).
4. A valid Modbus RTU **read request** frame is exactly 8 bytes:
   `[slave addr] [function code] [start addr hi] [start addr lo] [count hi] [count lo] [crc lo] [crc hi]`.
   Function code `0x03` = read holding registers, `0x04` = read input
   registers.
5. Match the `[slave addr]` byte and `[start addr]`/`[count]` fields against
   what your inverter is actually requesting, and adjust `modbus_slave_id`
   and the register map in the main YAML accordingly if they differ.

## CRC16 (Modbus)

The component implements the standard Modbus CRC16 (polynomial 0xA001,
initial value 0xFFFF) — the same algorithm essentially every Modbus RTU
device uses. Frames that fail the CRC check are counted in the
`Modbus CRC Errors` sensor and silently ignored (no response sent), matching
standard Modbus slave behavior (a slave never replies to a corrupted
request).

## Other traffic you might see on a shared RS485 bus

If your inverter also talks to other Modbus devices (e.g. additional power
meters, or a battery BMS that happens to share the same physical bus against
the manufacturer's recommendation), you will see other 8-byte frames with
different slave addresses in the logs — these are simply ignored by this
component, since it only answers to its own configured `slave_address`. If
you see much shorter frames (3–4 bytes) that don't look like valid Modbus at
all, this is most likely either bus noise/transceiver-switching artifacts, or
frame fragmentation caused by `frame_timeout_ms` being too tight for your
bus's timing — try raising it (e.g. to 15–20 ms) if logging/diagnostics
matter to you; it does not affect the actual Modbus slave responses.
