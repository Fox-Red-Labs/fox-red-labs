# motorlink: read an AM13E230 motor's live state over USB serial

Terms: [docs/vocab.md](../../docs/vocab.md).

Reads a TI AM13 motor solution's live state from a Mac or PC over the LaunchPad kit's USB serial port, and lets an AI assistant do the same through MCP. It speaks TI's MotorVision UART protocol, which ships in the AM13E2x SDK.

motorlink is this repo's own code, written from the protocol source in the SDK. It is not TI software or TI's MotorVision GUI, and TI does not endorse it.

## Credit and license

- The protocol is TI's: `source/realtimelib/utilities/motorvision` in the AM13E2x SDK 26.01.00.03, under TI's "Real Time Control Libs" license (use with TI devices only, notices kept).
- The firmware side used here is TI's Hall FOC solution as ported in `am13e230/foc-drv8376`. Its variable table is TI's `motorvision_config/motorvision_hash.c`, under BSD-3-Clause.
- This tool is for TI devices running that firmware.

## Use

Flash the solution's normal image (not the commissioning build), then:

```bash
python3 tools/motorlink/link.py poll
python3 tools/motorlink/link.py poll --names speed_Hz VdcBus_V --secs 5
```

`poll` prints the motor state and the fault word in words, plus the bus voltage, speed and phase currents.

For an AI assistant, register the MCP server, for example in a project's `.mcp.json`:

```text
{"mcpServers": {"motorlink": {"command": "python3", "args": ["tools/motorlink/mcp_server.py"]}}}
```

It offers three tools: `motor_status`, `motor_read` and `motor_variables`. The server is read-only. It cannot start the motor or change a gain. `link.py` can write variables, but it refuses to start the motor unless the caller passes `allow_run=True`.

The motor console (`site/motor-console.html`) is the same protocol in the browser, over Web Serial.

## What it measured (2026-10-17, LP-AM13E230 and DRV8376EVM, idle)

- The board streams at 9.2 Mbaud over the XDS110's USB serial port to a Mac. That was 299 kB in 0.5 s with nothing dropped by the probe.
- Polled values: the state reads "stop idle". The fault word reads 8192, a gate driver fault, because this build does not set the ILIMIT reference. The bus reads 12.44 V and the phase currents read under 3 mA.

## Two things the SDK files get wrong, and how motorlink handles them

- **The variable map disagrees with the firmware.** In every AM13 motor solution in SDK 26.01.00.03, `motorvision_hash.c` lists `VdcBus_V` three times, so it has 45 entries against the JSON's 43. From index 34 the JSON's numbers are two short: a host that trusts the JSON shows bus voltage as phase current. It also writes the wrong variable: asking for `startCurrent_A` writes `olSpeedThr`. motorlink takes indices and types from the C table and only permissions from the JSON.
- **The receiver never resynchronizes.** It counts bytes into 16-byte packets and never looks for the start byte. One lost byte makes it answer every later packet with PACKET_ERROR (39) until reset. motorlink writes every packet in full. On connect it sends a harmless probe, adding one filler byte at a time until the board acknowledges.

## Safety: the guard in the firmware

A host cannot keep a motor safe; the firmware has to. The normal image carries a bench guard (`am13e230/foc-drv8376/frl_guard.h`), published over this link as `frlGuard.*`:

- it refuses a start while TI reports a fault, while the bus is outside 10 to 14.5 V, or before the Hall table is calibrated;
- it stops a run at its cap (12 s by default, 60 s at most), 1 s after the host's last packet, on a TI fault or a bus out of range;
- it switches the driver off from the control interrupt when any phase current exceeds 1.5 A (a host may set 0.3 to 1.8 A), or when the background loop stops for 200 ms;
- it clamps the speed reference to 40 Hz electrical.

A host keeps a run alive by writing `frlGuard.heartbeat` every 250 ms.

Tested on the bench, 2026-10-17: Hall calibration in 171.2 s; a 12 s run at 10 Hz reached closed loop at 8.9 s and stopped at the cap with a 0.57 A peak; with the host cut off, the motor stopped 1.0 s after the last packet; 708 writes during 180 s of streamed polls arrived with no error.

## A third SDK problem: the receiver can stop for good

TI's receive interrupt handler drains the FIFO, then clears the interrupt. A byte that lands in between loses its interrupt, and the board stops hearing the host. The port clears first, then drains, and drains from the background loop every millisecond as well. The client also resyncs on PACKET_ERROR and on a second without poll replies, and treats a silent board as lost.

Polled booleans come back as 2 bytes.

## Motor campaign (2026-10-17)

`campaign.py` runs bounded tests through this client under the board's guard and keeps the raw byte stream of every run; `campaign_analyse.py` and `campaign_rows.py` turn the streams into rows in `data/results/am13e230.csv`.

- The board streams about 2,260 packets a second in all, so each variable is polled at about 280 samples a second with 8 variables and 375 with 6. The serial bridge delivers them in bursts about 65 ms apart, so sample times are rebuilt from the steady rate, not from when a read arrived.
- TI's `speed_Hz` updates once per Hall edge and has isolated spikes up to hundreds of Hz; a speed ripple against its own mean is meaningless, so the Hall-edge mean (6 edges per electrical revolution) is the denominator.
- With TI's default gains the closed loop is not reliable: 3 of 14 runs at 20 Hz reach a 20 s cap, 7 of 7 reversals fault. Details in the results rows.
