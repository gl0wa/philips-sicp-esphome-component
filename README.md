# Philips SICP ESPHome Component

ESPHome external component for controlling Philips displays over RS-232 using
the SICP protocol with the extended framing described below.

Tested compiler: ESPHome 2026.8.2 (ESP32, Arduino framework).

## What it does

- Sends SICP commands over UART through a TTL-to-RS-232 level shifter.
- Queues commands so only one is outstanding at a time (500 ms timeout,
  configurable retries, no blocking delays).
- Polls display state round-robin (one query per `update_interval` tick).
- Exposes native ESPHome entities: switch, select, number, sensor.
- Logs raw RX/TX frames at DEBUG/VERY_VERBOSE for protocol diagnosis.

## Wiring

```text
ESP32 (TTL UART) <-> level shifter (e.g. MAX3232 class) <-> display RS-232 (DB9)
```

> **WARNING:** The ESP32 UART is TTL level. It **MUST NOT** be connected
> directly to the RS-232 port. Always use a TTL-to-RS-232 level shifter.

Display DB9 (male, outside view), per SICP documentation:

| Pin | Signal | Direction              |
|-----|--------|------------------------|
| 2   | RXD    | Input to display       |
| 3   | TXD    | Output from display    |
| 5   | GND    | Ground                 |

A null-modem / crossover cable (2 <-> 3, 5 <-> 5) is normally required between
the host controller side and the display. If you get no replies, the first
thing to check is that TX and RX are actually crossed correctly; swap them and
try again. Verify with the UART debug output before assuming a protocol issue.

Typical UART settings (set on the `uart:` bus, not in this component):

```yaml
uart:
  id: display_uart
  tx_pin: GPIO17  # example only, adapt to your wiring
  rx_pin: GPIO16  # example only, adapt to your wiring
  baud_rate: 9600
  data_bits: 8
  parity: NONE
  stop_bits: 1
```

## Installation

```yaml
external_components:
  - source: github://gl0wa/philips-sicp-esphome-component
    components: [philips_sicp]
```

See `examples/display.yaml` for a full generic example.

## Configuration

```yaml
philips_sicp:
  id: display
  uart_id: display_uart
  update_interval: 30s
  # Optional timing tuning:
  # command_timeout: 500ms
  # max_retries: 2
  # command_gap: 60ms

  power:
    name: "Display Power"
  input:
    name: "Display Input"
  volume:
    name: "Display Volume"
  picture_format:
    name: "Display Picture Format"
  brightness:
    name: "Display Brightness"
  contrast:
    name: "Display Contrast"
  sharpness:
    name: "Display Sharpness"
  operating_hours:
    name: "Display Operating Hours"
  temperature:
    name: "Display Temperature"
  pip:
    name: "Display PIP"
  pip_position:
    name: "Display PIP Position"
  pip_source:
    name: "Display PIP Source"
```

Every entity is optional; configure only what you need. Polling automatically
covers only enabled entities, one query per update tick (no bursts).

## Entities

| Key               | Type   | SICP                                  | Notes                                                        |
|-------------------|--------|---------------------------------------|--------------------------------------------------------------|
| `power`           | switch | SET `18`, GET `19`                    | `01` = off, `02` = on. Bidirectional via GET report.         |
| `input`           | select | SET `AC`, GET `AD`                    | VGA, DVI, HDMI, MHL-HDMI2, DisplayPort, Mini DisplayPort.    |
| `volume`          | number | SET `44`, GET `45`, 0–100             | `0` mutes per documentation.                                 |
| `picture_format`  | select | SET `3A`, GET `3B`                    | Normal, Custom, Real, Full, 21:9, Dynamic (values 0–5).      |
| `brightness`      | number | SET `32` / GET `33`, 0–100            | Shared video SET; uses read-modify-write cache.              |
| `contrast`        | number | SET `32` / GET `33`, 0–100            | Same as above.                                               |
| `sharpness`       | number | SET `32` / GET `33`, 0–100            | Same as above.                                               |
| `operating_hours` | sensor | GET `0F 02`, 16-bit MSB/LSB, hours    | Read-only.                                                   |
| `temperature`     | sensor | GET `2F`, degrees C                   | Read-only; unsupported displays answer `00 03`.            |
| `pip`             | switch | SET `3C`                              | Enable/disable; position preserved. No GET defined.          |
| `pip_position`    | select | SET `3C`                              | Bottom/Top Left/Right (0–3). Optimistic + SET echo.          |
| `pip_source`      | select | SET `84`, GET `85`                    | Same input list; report parsing is best-effort (see below).  |

Picture-format mapping used here is Normal = 0, Custom = 1, Real = 2,
Full = 3, 21:9 = 4, Dynamic = 5. Some generic documentation tables list a
different order; this component uses the mapping above.

Input SET payloads are 5 bytes `AC <type> <number> 01 00` with
VGA `05 00`, DVI `07 01`, HDMI `06 02`, MHL-HDMI2 `06 03`,
DisplayPort `09 04`, Mini DisplayPort `09 05`.

## Protocol

TX uses this extended framing (verified on hardware for power commands):

```text
A6 01 00 00 00 | SIZE | 01 | COMMAND... | CHECKSUM
SIZE = len(COMMAND) + 2
CHECKSUM = 0xA7 ^ SIZE ^ 0x01 ^ each COMMAND byte
```

Known-good vectors (also covered by `tests/test_protocol.py`):

```text
Power ON : cmd 18 02            -> A6 01 00 00 00 04 01 18 02 B8
Power OFF: cmd 18 01            -> A6 01 00 00 00 04 01 18 01 BB
Volume 50: cmd 44 32            -> A6 01 00 00 00 04 01 44 32 D4
Input HDMI: cmd AC 06 02 01 00  -> A6 01 00 00 00 07 01 AC 06 02 01 00 08
```

The HDMI checksum above is the algorithmic `08`, verified working on
hardware (a legacy recording ends in `0F`; both appear to be tolerated, but
the component emits the correct `08`).

Replies from the display use the same layout with a different magic byte and
one fewer header zero:

```text
21 01 00 00 | SIZE | 01 | RESPONSE... | CHECKSUM
SIZE = len(RESPONSE) + 2
CHECKSUM = XOR of every preceding frame byte
```

Captured examples (volume 50, picture format Normal, power ON):

```text
21 01 00 00 04 01 45 32 52
21 01 00 00 04 01 3B 00 1E
21 01 00 00 04 01 19 02 3E
```

Two deviations from the generic SICP document were found on hardware and are
handled by the component:

- Successful SET commands are acknowledged with `00 00`, not the documented
  `00 06`. Both values are accepted as ACK; `00 15` (NACK) is retried and
  `00 18` (NAV) completes the attempt.
- A temperature GET (`2F`) on a display without that sensor is answered with
  the undocumented comm-control value `00 03`. Unknown comm-control values
  complete the attempt without retrying, and the temperature entity simply
  remains without state on such displays.

RX handling accepts both the extended framing above and the documented
generic SICP framing (`MsgSize Control Data... Checksum`, XOR checksum) as a
fallback, logs every accepted frame at DEBUG with hex, logs checksum failures
as warnings, and never spams INFO with raw packets. GET reports update
entities; GET polling sends one query per update tick; a command is retried up
to `max_retries` after `command_timeout` (default 500 ms, per SICP guidance).

## Tested vs documentation-derived

Verified on hardware (byte-for-byte TX, live RX decode, state sync):

- TX framing and checksum for power on/off, volume SET/GET, input GET/SET
  (including the HDMI payload), picture format GET, video GET, operating
  hours GET, PIP source GET.
- RX framing: `21` magic, XOR-over-frame checksum, `00 00` SET success reply.
- Entity state sync for power, input, volume, picture format, brightness,
  contrast, sharpness, operating hours, PIP source.

Documentation-derived and untested: DVI input mapping (`AC 07 01 01 00`),
PIP enable/position SETs, PIP source SET length, temperature (answered with
`00 03` on the tested display, i.e. unsupported there). The temperature,
PIP enable/position entities are included for displays that support them but
could not be exercised here.

## Troubleshooting

1. Enable detailed logging:

   ```yaml
   logger:
     level: DEBUG  # or VERY_VERBOSE for raw RX bytes
   ```

2. Check `TX SICP ... -> wire ...` lines: they prove the component is sending.
3. Check `RX ...` lines: no RX at all means a wiring problem (TX/RX swap,
   missing GND, wrong baud, shifter power) rather than a protocol problem.
4. `Timeout waiting for reply` + `retrying` means TX works but the display
   never answers (wrong address/framing, display asleep, wiring).
5. `NAV` means the command is understood but not available in the current
   display state. `NACK` means corruption; check baud/cabling.
6. Start conservatively with GETs (power/input/volume queries) before
   exercising SETs, and avoid repeated power cycling during tests.

## Contributing

Issues and pull requests are welcome. Please include DEBUG logs with raw
frames (redact nothing except your own network credentials, which never appear
in these logs), your ESPHome version, and which entities you exercised. Do not
submit private hostnames, IPs, Wi-Fi credentials, API keys, or device logs
containing them.
