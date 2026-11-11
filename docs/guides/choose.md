# Which radio and which software should I pick for an MSPM0 or CC23xx board?

Terms: [docs/vocab.md](../vocab.md). Errors and fixes: [cc2340r5/FAQ.md](../../cc2340r5/FAQ.md). Site version: `site/choose.html`. Generated from `site/data/choose.json` by `tools/render_choose.py`; edit the JSON, not this file. Checked 2026-10-06. 1 kB = 1,024 bytes.

For a battery sensor that talks to a phone, pick Bluetooth LE. For a smart home you already run with a hub, pick Zigbee or Thread. For Apple Home, Google Home or Alexa, pick Matter, which runs over Thread or Wi-Fi. Pick Wi-Fi when the device is plugged in, and Sub-1 GHz when it is far from the house. For software, TI's SDK has the radio stacks and TI's compiler. On CoreMark at -O3 that compiler was 26 percent faster than GCC: 2.30 against 1.82 per MHz on the LP-MSPM0G3507. Zephyr lets the same code move between chips and vendors. With the same GCC compiler it cost us 4 to 8 percent on one LP-MSPM0G3507. The C library also differs.

Contents: the radios side by side; where Matter fits; Zigbee or Thread; each radio; more than one radio; Zephyr or TI's SDK; what next.

## The radios side by side

Matter is not on this table because it is not a radio. It runs over Thread or Wi-Fi, covered in the next section. Power figures come from data sheets at different voltages and power levels, so compare sizes and ignore exact ratios. Range depends on your walls more than the radio. 1 kB = 1,024 bytes.

| | Bluetooth LE | Zigbee | Thread | Wi-Fi | Sub-1 GHz |
|---|---|---|---|---|---|
| What else you need | A phone, or a Bluetooth adapter or proxy for Home Assistant | A Zigbee hub, such as a USB coordinator radio on Home Assistant | A Thread border router | Your Wi-Fi router | A second Sub-1 GHz radio to receive it |
| On a battery | Yes: a CC2340R5 sleeps at 0.71 µA | Yes, as an end device | Yes, as a sleepy end device | Hard: about 0.4 to 1 mA just to stay connected. CC3235SF 710 µA; CC3551E 430 to 975 µA, target values | Yes, for small readings |
| Sending, current | 5.1 to 10.7 mA (CC2340R5) | Same 2.4 GHz chips as Bluetooth | Same 2.4 GHz chips as Bluetooth | 217 to 286 mA (CC3235SF) | Not compared |
| Devices relay for each other | Not by default | Yes: mains-powered devices route | Yes: routers stay on, so they are mains powered | No: everything goes through your router | Depends on the protocol |
| Gets an IP address | No | No: the hub translates | Yes, IPv6 | Yes: IPv4, and IPv6 if your network has it | Depends on the protocol |
| Apple Home, Google Home, Alexa | Not checked for a home-made board; Matter is the standard path | Through a hub | Through Matter | Through Matter or the maker's app | No |
| Home Assistant | Bluetooth integration, BTHome | ZHA or Zigbee2MQTT | Thread integration, usually with Matter | MQTT, ESPHome and others | Through your own bridge |
| What was measured | 45 ms round trip on LP-EM-CC2340R5 | 95 ms per command from Home Assistant at first pairing; about 5 s after a re-pairing on LP-EM-CC2340R5 | Thread device builds did not fit 36 kB of RAM; they build completely for 64 kB and were not run | LP-EM-CC35X1 joined Wi-Fi and its ESPHome API answered 20 of 20 commands; Home Assistant adoption not run | Not measured |

## Where Matter fits

Matter is not a radio. It is a common language for smart-home devices, from the Connectivity Standards Alliance, that runs on IP networks: Wi-Fi, Thread or Ethernet. Bluetooth LE only sets a new device up. So you choose between Matter over Thread, which is low power and needs a border router, and Matter over Wi-Fi, which needs no extra box and uses more power. One Matter device can sit in Apple Home, Google Home, Alexa and Home Assistant at once. Matter needs a big part. TI marks Matter over Thread on the CC2755P10 as evaluation, and Matter over Wi-Fi on the CC3551E as production. The CC3551E board is the LP-EM-CC35X1. TI offers no Matter build for the CC2340R5.

## Zigbee or Thread: choosing between them

Both are low-power meshes on 2.4 GHz, good for battery sensors, and both work with Home Assistant. Pick Zigbee if you have a Zigbee hub or USB radio already. Pick Thread if you have a Thread border router, or want Matter on battery devices. Many Apple and Google home hubs are border routers, and Matter's low-power path is over Thread. Thread devices get IPv6 addresses; Zigbee devices need the hub to translate. On TI chips, Thread needs more memory in TI's builds: on the 36 kB CC2340R52 Thread device builds did not fit and Zigbee did.

## Bluetooth LE: when to pick it

Talks straight to a phone or computer, no hub. A CC2340R5 sleeps at 0.71 µA between radio events (data sheet).

Pick it when:

- A phone app should talk to it directly
- It runs for a long time on a coin cell
- You want readings in Home Assistant through a Bluetooth adapter or an ESPHome Bluetooth proxy (BTHome is an open format it reads)

Skip it when:

- Messages need to hop across the house through other devices: Zigbee and Thread do that by default
- It should appear in Apple Home or Google Home on its own: use Matter

- **Power:** 5.1 to 10.7 mA sending, 0.71 µA asleep (CC2340R5 data sheet, 3.0 V)
- **Address:** No IP address; a phone or hub talks to it
- **What was measured:** LP-EM-CC2340R5: on air 1.40 s after flashing, connects to a Mac in 0.55 s, 45 ms round trip for 20 bytes.

## Zigbee: when to pick it

A low-power mesh for lights, plugs and sensors, run by a hub. Mains-powered devices relay messages for the others.

Pick it when:

- You already have a Zigbee hub, such as Home Assistant with a USB Zigbee radio
- Battery sensors spread across a house
- You want it in Home Assistant through ZHA or Zigbee2MQTT

Skip it when:

- You have no hub and do not want one
- It should appear in Apple Home or Google Home without a bridge: use Matter

- **Channels:** Home Assistant suggests Zigbee channel 15, 20 or 25 "to avoid interoperability problems with Zigbee devices". Zigbee shares 2.4 GHz with Wi-Fi. Wi-Fi channels 1, 6 and 11 overlap Zigbee channels 11 to 14, 16 to 19 and 21 to 24 (from the channel frequencies), and 15, 20 and 25 sit in the gaps. That is the US Wi-Fi plan; in Europe, Wi-Fi channel 13 overlaps Zigbee 22 to 26
- **Address:** No IP address; the hub translates
- **Words:** A coordinator is the hub's radio. Mains powered means plugged into the wall. Zigbee2MQTT and ZHA are the two ways Home Assistant runs a Zigbee network
- **What was measured:** LP-EM-CC2340R5 as a Zigbee light in Home Assistant: 20 of 20 commands, 95 ms median at first pairing (2026-10-05); about 5 s per command after a re-pairing two hours later; the 2026-10-16 repeat did not join. Suspected: the radio environment. TI's example listened on channel 11 only until all channels were set. That default suits a bench, not a home.

## Thread: when to pick it

A low-power mesh like Zigbee, but every device gets an IPv6 address. It reaches your network through a border router.

Pick it when:

- You have a border router already: Apple TV 4K (2nd generation, or 3rd generation Wi-Fi + Ethernet; not the Wi-Fi-only model), HomePod mini, HomePod (2nd generation), Nest Hub (2nd gen), Nest Hub Max, Nest Wifi Pro, Google TV Streamer (4K), or Home Assistant with a Connect ZBT-1 (formerly SkyConnect) or ZBT-2 running the OpenThread Border Router app
- You plan to use Matter on battery devices
- Battery sensors that sleep: sleepy end devices keep the radio off most of the time

Skip it when:

- There is no border router in the house and you do not want one
- Your chip is the 36 kB CC2340R52: TI's Thread device builds did not fit it, while its Bluetooth and Zigbee examples did

- **Routers:** Keep the radio on all the time, so in practice they are mains powered (plugged in). A sleepy end device keeps its radio off most of the time
- **Address:** IPv6, inside and outside the mesh
- **On TI chips:** TI lists Thread device roles on the CC2340R53 (evaluation) and the CC27xx chips (production). On the CC2340R5 TI lists only the radio co-processor role, marked evaluation
- **What was measured:** Thread device builds for the LP-EM-CC2340R5 (36 kB of RAM) needed 7.5 to 23.8 kB more RAM. Suspected: a chip limit. Only TI's configurations were built; a trimmed build might fit. They link for the 64 kB CC2340R53 and were not run.

## Matter: when to pick it

Not a radio. A common language over Thread, Wi-Fi or Ethernet, so one device works with Apple Home, Google Home, Alexa, SmartThings and Home Assistant at once.

Pick it when:

- It should work in any smart-home app, or several at once
- Other people, who may not run Home Assistant, will use it

Skip it when:

- Only you will use it, with Home Assistant: Zigbee or Bluetooth is simpler
- The chip is small. TI offers Matter only on bigger wireless parts, such as the CC2755P10 (Thread, evaluation) and the CC3551E on the LP-EM-CC35X1 board (Wi-Fi, production)

- **Runs over:** Thread or Wi-Fi (or Ethernet); Bluetooth LE only to set it up
- **What else you need:** A Matter controller (Apple Home, Google Home, Alexa or Home Assistant), and a border router for Thread devices
- **What was measured:** Not measured. TI offers no Matter build for the CC2340R5.

## Wi-Fi: when to pick it

Joins your home network directly. No hub and plenty of speed, at much more power.

Pick it when:

- It is plugged into the wall
- It sends a lot of data, or talks to a web service
- You want it on the network today with no hub, for example over MQTT into Home Assistant

Skip it when:

- It runs on a small battery: staying connected alone costs about 0.4 to 1 mA

- **Power:** 217 to 286 mA sending, 53 to 74 mA receiving, 710 µA staying connected (CC3235SF data sheet, 3.6 V). The newer CC3551E lists 430 to 975 µA connected, as target values
- **Band:** 2.4 GHz and often 5 GHz
- **TI boards:** LAUNCHXL-CC3235SF (Wi-Fi 4), LP-EM-CC35X1 (Wi-Fi 6 with Bluetooth)
- **What was measured:** Ran on the LP-EM-CC35X1: it joins Wi-Fi and its ESPHome API answered 20 of 20 commands from a client on the board. Home Assistant adoption is not run because the test network keeps clients apart. Evidence: docs/experiments/2026-10-13-esphome-cc35xx-platform.md.

## Sub-1 GHz long range: when to pick it

A lower frequency that reaches farther and passes walls better, for sensors at the far end of a garden or a farm.

Pick it when:

- The sensor sits past where 2.4 GHz reaches. In TI's example, the same radio performance gives 292 m at 868 MHz against 233 m at 2.4 GHz, and about twice the range through an 8-inch concrete wall
- It sends small readings, not much data

Skip it when:

- A phone or a standard smart-home app should hear it directly: phones have no Sub-1 GHz radio
- You read about 100 km records. Those need a slow long-range mode, high antennas and a clear line of sight

- **What else you need:** A second Sub-1 GHz radio at home to receive it, such as another TI board or a BeaglePlay
- **TI boards:** LP-CC1354P10-1, LP-CC1352P7-1
- **What was measured:** Not measured. An LP-CC1354P10-1 is on this bench.

## Running more than one radio

Yes, with limits. A bigger chip can run two radio stacks at once: TI lists Bluetooth together with TI 15.4 as shipping on the CC2755P10. TI's release notes say the CC23xx chips do not support two stacks at once. TI's two-radio example for the CC2755P10, Bluetooth with TI 15.4, was moved to the 36 kB CC2340R52. It did not link: 2.1 kB of RAM short even before its heap. Suspected: a chip limit. Only TI's example was moved. A board can also carry two radio chips: the BeaglePlay has Wi-Fi plus a CC1352P7 for Bluetooth, Zigbee and Sub-1 GHz. Or let a hub join them: Home Assistant takes Zigbee, Thread, Bluetooth and Wi-Fi devices into one place. A CC2340R5 can also run Bluetooth or Zigbee, one at a time, by flashing different firmware.

## Zephyr or TI's SDK on a TI chip (MSPM0, CC2340R5)

You can write code for a TI board with TI's own SDK or with Zephyr. On 2026-10-06, PlatformIO's board list and MicroPython's download page did not list the MSPM0 or CC23xx boards. The checks ran on an Intel Mac running macOS 15.7.9, with Zephyr 4.4.2 and GCC 12.2, TI Arm Clang 5.1.1, SimpleLink Low Power F3 SDK 9.21.00.36 and MSPM0 SDK 2.11.00.07. The speed gap is mostly the compiler. With the same GCC compiler, Zephyr cost 4 to 8 percent on CoreMark, on one LP-MSPM0G3507. The C library also differs. The time-to-blink numbers below are not like for like.

| | TI's SDK | Zephyr |
|---|---|---|
| What it is | TI's drivers, examples and radio stacks, set up with SysConfig, TI's point-and-click tool | An open-source operating system, a Linux Foundation project, with one build tool (west) and one driver model across more than 1,000 boards |
| Boards on the Hardware page | All have a TI SDK | 8 of the 27 are in upstream Zephyr, the main Zephyr project (2026-10-06) |
| Radios | Bluetooth LE, Zigbee and proprietary 2.4 GHz, which is TI's own packet format, in the Low Power F3 SDK; Thread and Matter as separate downloads | Upstream Zephyr has no Bluetooth for the CC2340R5. TI's fork, its own copy of Zephyr, adds it, but on the test board its samples flashed and printed nothing. The cause is not known. Suspected cause: tool or docs. One boot setting differs from upstream, and the fork is an older Zephyr (3.7), so it is one difference among several, not proven. |
| Speed, CoreMark per MHz, LP-MSPM0G3507 | 2.30 with TI Arm Clang, no operating system | 1.82 with GCC, the compiler Zephyr uses, no operating system, -O3 with loop unrolling |
| Same compiler, GCC 12.2 -O2, LP-MSPM0G3507 | 1.76, no operating system | 1.63 under Zephyr: 7.9 percent slower. The C library also differs. |
| Interrupt response, the time to react to a signal, LP-MSPM0G3507 | 29 to 36 CPU cycles, about 0.4 µs at 80 MHz, no operating system, TI Arm Clang | About 55 to 65 cycles net, roughly twice bare metal. 163 to 181 cycles under Zephyr with GCC, 99 to 126 of them Zephyr's slower timer read. |
| Moving to another vendor's chip | Start again with that vendor's tools | In the good case, change the board file, which describes the board's pins |
| Learning first | SysConfig is point and click | Devicetree, Kconfig and west; one training firm budgets two to four weeks for an experienced developer |
| On an Intel Mac | Works | Zephyr's guide says x86-64 macOS is not supported. The runs worked with workarounds, written up in mspm0g3507/zephyr/README.md |

### TI's SDK

TI's drivers, examples and radio stacks, set up with SysConfig.

Pick it when:

- You need a radio stack: Bluetooth, Zigbee, Thread and Matter come from TI
- You want TI's examples and its graphical setup
- Speed or a quick interrupt response matters: the fastest results came from TI's compiler with no operating system

Skip it when:

- You plan to move the same code to another vendor's chips
- You want one way of working across many vendors' boards

What was measured: LP-EM-CC2340R5: builds in 22 s with make and flashes in 7 s. Not like for like with the Zephyr times.

### Zephyr

An open-source operating system that runs the same way on many vendors' chips.

Pick it when:

- You may switch chips or vendors later
- You want one tool and one way of working across boards
- You want open source with a large community

Skip it when:

- You need Bluetooth on the CC2340R5 today
- Your board is not in upstream Zephyr: see each board's page
- You need the fastest code: TI's compiler, which Zephyr does not use, was 26 percent faster than GCC at -O3 on CoreMark (LP-MSPM0G3507) and was the bigger part of the speed gap

What was measured: LP-MSPM0G3507: 3 minutes from clean to blinking with this repo's script; 9.5 minutes the first time, while the workarounds were found. LP-EM-CC2340R5: blinking 82 s after the first command, with Zephyr installed. Not like for like with the TI SDK times.

## Three common questions, answered from the rows

Three common questions. Each answer gives the requirement, a configuration that ran, what limits it, and what no row establishes. Every number in the tables is read from the results store when the page is built. A constraint that is not measured names the experiment that would decide it, and until then the answer is a candidate. Terms: ZHA is Home Assistant's Zigbee integration. LQI and RSSI are the link quality and signal strength a radio reports. NNC is TI's neural network compiler. EnergyTrace is the current meter in TI's debug probe. FOC is field-oriented control, the usual motor-control math. The TMU is the C2000's hardware for sine, cosine and division.

### A Zigbee light or switch in Home Assistant: which TI board, and whether it responds fast enough

Requirement: A device that joins an existing Zigbee network run by Home Assistant (ZHA) and acts on a command within about a second.

**LP-EM-CC2340R5 with TI's onoff_light example from the SimpleLink Low Power F3 SDK, built as a router, plus the LP-XDS110ET debugger to program it.** (candidate, not a recommendation). It joined ZHA (Home Assistant's own Zigbee integration) 37 s after the flash finished, and 20 of 20 on/off commands from Home Assistant landed, median 95 ms, read from Home Assistant's own state. That meets the requirement on the first pairing. After a re-pairing two hours later the median was about 5 s, which does not, so the one-second part is met once and missed once. A repeat on 2026-10-16 did not join Home Assistant in four tries; until a new run passes, this is a record of one run, not a tested route.

Limiting factor: RAM, and the link. The router image leaves 799 B of the chip's 36 kB free, so there is almost no room for your own code in that build; the end-device image leaves 6.4 kB. The 5 s medians came with a weaker link (LQI, the link quality the radio reports, 36 to 48 against 60 at first). Cause unknown (suspected: radio environment).

| What | Value | Runs | Checked by | Date | Row's source |
|---|---|---|---|---|---|
| Join time, flash finished to listed in ZHA | 37 s | 1 | ha-state | 2026-10-05 | cc2340r5/zigbee/README.md |
| Command latency from Home Assistant, first pairing | 95 to 292 ms (median of 20 commands; the slowest was 292 ms) | 1 | ha-state | 2026-10-05 | cc2340r5/zigbee/README.md |
| Commands that landed, first pairing | 100 % | 1 | ha-state | 2026-10-05 | cc2340r5/zigbee/README.md |
| Command latency after a re-pairing two hours later | 5000 to 7000 ms (median of 30 commands with a weaker link; the slowest was 7 s) | 1 | ha-state | 2026-10-05 | cc2340r5/matrix/benchmarks.md |
| RAM left in the router image | 799 B (linker map, logging on) |  | linker | 2026-10-05 | cc2340r5/matrix/memory.md |
| Flash used by the router image | 275.7 kB |  | linker | 2026-10-05 | cc2340r5/zigbee/onoff_light/freertos/ticlang/onoff_light.map |

- Joins Home Assistant and takes commands: measured: yes, 20 of 20 at 95 ms median on the first pairing
- Stays joined and responsive for days: unmeasured. leave it paired for 72 hours with a command every minute and log the misses and the latency; the re-pairing figure (about 5 s) says the link can degrade, and no long run exists
- Battery life as a sleepy device: unmeasured. build the end-device image, measure sleep current and energy per command with the EnergyTrace probe at a stated voltage, then compute days from a named cell; nothing here was measured on a battery
- Room for your own code: measured: 799 B of RAM free in the router build and 6.4 kB in the end-device build (linker map); a real product needs the end-device build or a 64 kB part

Conditions:

- SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1, FreeRTOS, the SDK makefile's default flags
- LP-EM-CC2340R5, board revision not recorded, probe firmware 3.0.0.43; a previous owner's board
- Home Assistant with ZHA and a ZBT-2 coordinator on a Mac mini; LQI 60, RSSI -85 dBm at the first pairing
- measured 2026-10-05 from an Intel Mac, macOS 15.7.9

Not established:

- long-term reliability (one evening, two pairings)
- any battery figure
- behavior on the LP-EM-CC2340R53 TI sells now (64 kB RAM; the board tested is no longer listed)
- Apple Home, Google Home or Alexa: Zigbee reaches Home Assistant only; Matter is the route to those

Next: Before buying for this, decide the unmeasured constraint: Stays joined and responsive for days: leave it paired for 72 hours with a command every minute and log the misses and the latency; the re-pairing figure (about 5 s) says the link can degrade, and no long run exists; Battery life as a sleepy device: build the end-device image, measure sleep current and energy per command with the EnergyTrace probe at a stated voltage, then compute days from a named cell; nothing here was measured on a battery Reproduce: project-zigbee-light.html Errors and fixes: faq-cc2340r5.html#how-much-ram-is-left-for-your-code-after-the-radio-stack

### A small on-chip classifier under 5 ms per answer on a coin-cell budget: whether the MSPM0G5187's NPU reaches it

Requirement: A 4-class classifier on 4 sensor channels by 32 samples, answered in under 5 ms, in a few kB of flash and RAM, with the model's accuracy known.

**LP-MSPM0G5187 with the 8.5k-parameter occupancy model compiled by TI's NNC for the NPU, at 80 MHz.** (candidate, not a recommendation). The answer takes 2.12 ms on the NPU (the chip's neural processing unit) at 80 MHz, so the 5 ms budget is met twice over. The model objects take 14.0 kB of flash and 1.4 kB of RAM. The 20 stored test vectors give the same class on the board as on the Mac, so the port is right. For scale: TI's own CPU build of the same model (NNC, TI's neural network compiler) takes 45.64 ms, 21.6 times longer, and the fastest plain C build 35.27 ms; that last pair is not like for like because the plain C time also includes input quantization.

Limiting factor: What the model knows, not the chip. Balanced accuracy is the average of how often each class is caught. On a day the model never trained on it is 87.7 percent accurate overall but calls 159 of 170 three-person windows empty, a balanced accuracy of 40.2 percent; the 90.9 percent figure comes from a same-day split. And the time is the model call only: no sensor read, no sleep or wake, so it is not a pipeline latency.

| What | Value | Runs | Checked by | Date | Row's source |
|---|---|---|---|---|---|
| Model call on the NPU, 80 MHz | 2.116 to 2.155 ms (fastest of 20 vectors, best of 5 calls (best case); value_hi is the slowest vector; model call only) | 3 | vectors | 2026-10-09 | mspm0g5187/bench/ai/repeat.sh |
| The same model, TI's CPU build (NNC), 80 MHz | 45.644 to 45.675 ms (the like-for-like baseline) | 3 | vectors | 2026-10-09 | mspm0g5187/bench/ai/repeat.sh |
| Fastest plain C build, 80 MHz | 35.273 to 35.331 ms (includes input quantization; not like for like) | 1 | vectors | 2026-10-08 | mspm0g5187/bench/ai/run.sh |
| Model flash, NPU build | 14 kB (model objects from the linker map; the image around them is extra) | 1 | linker | 2026-10-09 | data/ai-model-sizes.csv |
| Model RAM, NPU build | 1.4 kB | 1 | linker | 2026-10-09 | data/ai-model-sizes.csv |
| Energy above idle per inference, NPU | 4.23 to 4.92 uJ (steady loop, current above the board's idle reading times the best-case time; spreads 7 to 32 percent between runs) | 3 | by-us | 2026-10-09 | mspm0g5187/bench/power/energy_occ.py |

- Under 5 ms per answer: measured: 2.12 ms best case, 2.16 ms slowest vector, model call only
- Accuracy on data it did not train on: measured: 90.9% on a same-day block split; 87.7% overall and 40.2% balanced with whole days held out (tools/dataset/eval_occupancy.py); one room only
- Coin-cell life: unmeasured. measure the sleep current with the board's isolation shunts changed and the chip in STANDBY, then energy per wake-infer-sleep cycle with EnergyTrace at a stated voltage; today only a steady loop above a 16.4 mA board floor was measured, which says nothing about a sleeping product
- Works on your own sensor data: unmeasured. record your own sessions with tools/dataset/capture.py, train with the same recipe, and run the day-held-out evaluation; the model here learned one room

Estimates beside the result (added up, not tested together):

- The model's 14.4 kB of flash and 1.4 kB of RAM are the model objects only; TI's NPU example image around them, clock setup and startup included, is a separate measured image. Adding your sensor driver and a sleep loop is an estimate until it is one built image.

Conditions:

- MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS at -O2, TI NNC 2.1.2 for the NPU and CPU builds, int8
- LP-MSPM0G5187 Rev. A, 80 MHz from the SysConfig of TI's motor_bearing_fault_detection_ai_npu example
- three runs, each a full erase and flash, agreeing to the cycle; 2026-10-09; Intel Mac, macOS 15.7.9

Not established:

- any pipeline time (sensor to decision)
- any sleep or wake figure, so any battery life
- accuracy in another room

Next: Before buying for this, decide the unmeasured constraint: Coin-cell life: measure the sleep current with the board's isolation shunts changed and the chip in STANDBY, then energy per wake-infer-sleep cycle with EnergyTrace at a stated voltage; today only a steady loop above a 16.4 mA board floor was measured, which says nothing about a sleeping product; Works on your own sensor data: record your own sessions with tools/dataset/capture.py, train with the same recipe, and run the day-held-out evaluation; the model here learned one room Reproduce: project-small-ai-model.html Errors and fixes: ../mspm0g5187/FAQ.md#the-occupancy-model-scores-909-percent-on-its-test-set-but-much-less-on-a-new-day-why

### A 50 kHz motor-control loop: which board keeps up, and how much of the period is left

Requirement: A PWM-triggered loop (ADC sample, one field-oriented control step, new PWM value) that completes well inside a 20 us period, with the CPU time it takes known.

**LAUNCHXL-F2800137 (TMS320F2800137 at 120 MHz) with the TMU build of a control-loop harness, code in RAM.** (candidate, not a recommendation). Trigger to new PWM value takes 2.4 us mean (2.33 to 2.42 us over 1,000 interrupts) with the TMU, the chip's hardware for sine, cosine and division, in one field-oriented control (FOC) step. At 50 kHz the period is 20 us, so the loop uses about 12 percent of it. The loop still updates every period up to 336 kHz, and at 100 kHz it takes 26.4 percent of the CPU.

Limiting factor: This is one FOC step on a fixed input, not a motor: no current sensing chain, no observer, no fault handling, no communications. The 336 kHz figure is where the harness stops updating in time; your loop has more in it.

| What | Value | Runs | Checked by | Date | Row's source |
|---|---|---|---|---|---|
| Trigger to new PWM value, TMU, code in RAM | 2.4 us (mean of 1,000 interrupts; 279 to 290 cycles) | 1000 | by-us | 2026-10-06 | c2000-launchpad/bench/control-loop.md |
| Worst case as a share of a 10 us period (the 100 kHz benchmark period; halve it for 50 kHz) | 24.2 % | 1000 | by-us | 2026-10-06 | c2000-launchpad/bench/control-loop.md |
| Jitter, worst minus best | 0.092 us | 1000 | by-us | 2026-10-06 | c2000-launchpad/bench/control-loop.md |
| Highest rate that never misses a period | 336.13 to 336.13 kHz (measured on the board, 3 trials) | 3 | readback | 2026-10-08 | c2000-launchpad/bench/ctrl/build-tmu/run-fom.txt |
| CPU load at 100 kHz | 26.4 to 33.3 % (value_hi keeps the timing checks in) | 3 | readback | 2026-10-08 | c2000-launchpad/bench/ctrl/build-tmu/run-fom.txt |
| The same loop without the TMU (C library math) | 3.7 us (two compiler flags differ, not isolated) | 1000 | by-us | 2026-10-06 | c2000-launchpad/bench/control-loop.md |

- Completes inside a 20 us period: measured: 2.4 us mean, about 12 percent of the 20 us period; the benchmark row says 24.2 percent of its 10 us period
- Behaves with a real motor, current sensors and an observer: unmeasured. run TI's motor-control kit example with a motor and current sensing on this board and time the same trigger-to-PWM path inside it; the harness here is one FOC step on a fixed input
- Code in flash instead of RAM: measured: 2.58 us mean with the end-of-conversion interrupt from flash, 2.32 us with the loop copied to RAM at startup

Conditions:

- TI C2000 compiler 25.11.1.LTS at -O2, C2000Ware 26.02.00.00 LTS, --tmu_support=tmu0 and --fp_mode=relaxed for the TMU build
- LAUNCHXL-F2800137, board MCU109A, chip TMS320F2800137 silicon revision B, probe firmware 3.0.0.43; a used board
- ePWM1 50 kHz trigger, 9-cycle sample window, 1,000 interrupts; the rate sweep and CPU load on 2026-10-08; the loop times 2026-10-06; Intel Mac, macOS 15.7.9

Not established:

- anything with a motor attached
- the F28P65x or other C2000 parts (the LAUNCHXL-F28P65X has its own rows)
- the loop with communications or a second interrupt running

Next: Before buying for this, decide the unmeasured constraint: Behaves with a real motor, current sensors and an observer: run TI's motor-control kit example with a motor and current sensing on this board and time the same trigger-to-PWM path inside it; the harness here is one FOC step on a fixed input Reproduce: benchmarks.html#control Errors and fixes: faq-f2800137.html#how-fast-can-a-control-loop-run-on-the-f2800137

## Where to go next

Pick a board: open the Hardware options page and press the need you chose. Hit an error: the LP-EM-CC2340R5 FAQ lists each one with its cause. Not measured: Wi-Fi, Sub-1 GHz, Matter, indoor range and battery life.

## Sources

- Fact check with every quote, URL and date: docs/research/radio-and-software-choice-sources-2026-10-06.md
- Matter 1.4.1 core specification (CSA): https://csa-iot.org/wp-content/uploads/2025/05/23-27349-007_matter-1-4-1-core-specification.pdf
- Home Assistant Matter, Thread, ZHA and Bluetooth integrations: https://www.home-assistant.io/integrations/matter/, https://www.home-assistant.io/integrations/thread/, https://www.home-assistant.io/integrations/zha/, https://www.home-assistant.io/integrations/bluetooth/
- Apple Thread devices: https://support.apple.com/en-us/102078; Google hubs: https://support.google.com/googlenest/answer/12391458
- OpenThread node roles: https://openthread.io/guides/thread-primer/node-roles-and-types; BTHome: https://bthome.io/
- TI white paper SWAY034 (protocols compared): https://www.ti.com/lit/wp/sway034/sway034.pdf; TI app note SWRA479A (Sub-1 GHz range): https://www.ti.com/lit/an/swra479a/swra479a.pdf
- Data sheets: CC3235SF https://www.ti.com/lit/ds/symlink/cc3235sf.pdf, CC2340R5 https://www.ti.com/lit/ds/symlink/cc2340r5.pdf
- Zephyr: https://www.zephyrproject.org/, https://docs.zephyrproject.org/latest/develop/getting_started/index.html; Ac6 Training on Zephyr: https://www.ac6-training.com/en/articles/freertos-or-zephyr-choosing-an-rtos
- TI SDKs: https://github.com/TexasInstruments/simplelink-lowpower-f3-sdk, https://github.com/TexasInstruments/simplelink-zephyr
- CC3551E data sheet: https://www.ti.com/lit/ds/symlink/cc3551e.pdf; TI app note SWRA584 (Sub-1 GHz long-range mode): https://www.ti.com/lit/an/swra584/swra584.pdf
- TI on Matter, Thread and two radios per chip: docs/research/matter-on-ti-2026-10-06.md, docs/research/ti-wireless-stack-matrix-2026-10-05.md, docs/research/ti-portfolio-specs-2026-10-06.md; https://github.com/TexasInstruments/matter
- BeaglePlay and other boards: docs/research/ti-portfolio-gaps-2026-10-06.md; PlatformIO and MicroPython lists: docs/research/hobbyist-project-list-2026-10-06.md
- Measurements: mspm0g3507/mcu-bench/RESULTS-2.md, mspm0g3507/mcu-bench/RESULTS-cc2340r5.md, mspm0g3507/STOPWATCH.md, mspm0g3507/zephyr/README.md, cc2340r5/baremetal/README.md, cc2340r5/matrix/benchmarks.md, cc2340r5/zigbee/README.md, cc2340r5/zephyr/README.md, cc2340r5/zephyr-ti/README.md, docs/experiments/
- Zigbee and Wi-Fi channel frequencies: Zigbee channel n is centered at 2405 + 5 x (n - 11) MHz (IEEE 802.15.4); Wi-Fi channels 1, 6 and 11 are centered at 2412, 2437 and 2462 MHz and about 22 MHz wide
