# Vocab, in plain language

One line each, in the order a newcomer meets them. If a word in any doc in this project is not here, add it.

## Words on this site

**Works / Should work / Could work / Not for this chip.** The four status words on reader pages. Works: it ran here and did what it should; a note names any part that did not run. Should work: the same API is on both sides and only naming, packaging or build glue is in the way; the blocker is named. Could work: real porting effort, a missing block or a module to rewrite; the blocker is named. Not for this chip: the chip or its vendor software cannot do it (RAM, flash, CPU, no radio, no software). A more precise claim is the hover text.

**Tested with this setup / Expected to work / Needs adaptation / Not assessed.** The four assessment words behind the chips in a project's Other boards list. Tested with this setup: that configuration ran and passed the stated check. Expected to work: an engineering assessment says so; it has not run. Needs adaptation: the main work is named. Not assessed: nobody looked.

**Builds, Hardware, Software.** The menu items. Builds are projects you can repeat; Hardware holds the board picker, add-ons and Benchmarks; Software holds ports and the tools you already use.

**Show and tell.** The category on the repository's Discussions tab where you post what you built: what it is, where it runs, one proof, and what did not work.

**Reports from others.** The heading on a project or board page for rows that readers filed through the issue forms. A row is in the reporter's words and is never shown as a result measured in this lab.

**Built-by list.** A line at the foot of a board's `FAQ.md` naming the GitHub handles of people whose posts or reports led to a fix on that board.

**Digital Lab.** The browser bench that programs a connected board. Lab always has a capital L.

**Port.** Code written for another chip or ecosystem made to run on a LaunchPad kit, with the original files left as they shipped. Every port is listed on the Ports page with what ran and what only built.

**Vibe to Volts.** The home page headline: going from vibe coding to real analog and embedded projects on your bench.

**LaunchPad.** TI's name for a cheap development board with the chip, a USB connector, an LED, a button, and a built-in programmer. The thing you buy. Exception: TI's LP-EM wireless boards (such as the LP-EM-CC2340R5) have no USB connector or programmer on board; a separate probe clips on.

**BoosterPack.** A plug-in board that stacks on top of a LaunchPad to add a motor driver, a radio, a display, or sensors.

**MCU.** Microcontroller. A small computer on one chip, with its own memory and pins, running one program forever. MSPM0 and MSPM33 are TI's MCU families.

**Cortex-M0+ / M33.** The Arm processor core inside the chip. M0+ is the smallest and cheapest; M33 is faster and has security features. Your code does not care much which one it is; the tools do.

**SDK.** Software development kit. TI's zip of example projects, drivers, and configuration data for one chip family. You install it; the IDE reads from it.

**DriverLib.** TI's library of functions that poke the chip's hardware so you do not have to read the register manual. `DL_GPIO_togglePins` flips an LED.

**SysConfig.** TI's tool that generates the setup code for pins, clocks, peripherals and radio stacks. It has a point-and-click view and a command line (`sysconfig_cli.sh`). The `.syscfg` file in a project is its save file, plain text you can edit; the generated files start with `ti_` (for example `ti_msp_dl_config.c` on MSPM0, `ti_drivers_config.c` on CC23xx).

**CCStudio (Code Composer Studio).** TI's IDE. Version 21 is built on Theia, an open-source cousin of VS Code. It bundles the compiler, the debugger, SysConfig, and TI's MCP servers.

**Theia.** The open-source editor framework CCStudio is built on. It looks like VS Code and runs most VS Code extensions, but it is not VS Code.

**TI Arm Clang.** TI's compiler for Arm chips. Turns your C into a `.out` file the chip can run.

**XDS110.** The programmer and debugger chip built into every LaunchPad. It is what your Mac actually talks to over USB. It programs the MCU and lets a debugger pause it and read its memory. Exception: TI's LP-EM wireless boards (such as the LP-EM-CC2340R5) have no probe on board; you clip on a separate one, the LP-XDS110ET or LP-XDS110.

**SWD.** Serial Wire Debug. The two-wire connection between the XDS110 and the MCU. When a doc says SWCLK and SWDIO, that is these two wires.

**RAM.** The chip's working memory: where a running program keeps its variables, stacks and buffers. It is cleared at power-off, it is the smallest memory on these chips (1 kB to 296 kB here), and it is what runs out first.

**Flash.** The chip's program memory: where your program and its constants live, kept through power-off. Bigger than RAM (16 kB to 1 MB here), and slower to read at high clocks (see wait states).

**Linker map.** The file the build writes that lists every function and variable with its size and address; the memory figures on the site are read from it.

**Flashing.** Writing your program into the chip's flash memory so it runs on power-up. Also called programming or loading.

**Debugging.** Running the program under control: pause it, step one line at a time, look at variables. Needs the XDS110 and a debugger such as CCStudio's or GDB.

**GDB.** The GNU debugger. A command-line debugger that every open-source toolchain uses. OpenOCD and VS Code both talk to it.

**OpenOCD.** Open On-Chip Debugger. Free, open-source software that talks to debug probes like the XDS110 and lets GDB flash and debug the chip without the vendor's IDE. It needs a configuration file per probe and per chip; support for new chips arrives when someone writes those files. The Zephyr SDK ships an OpenOCD that does not know the XDS110, so TI's docs have you build a newer one.

**UniFlash.** TI's standalone flashing tool, with a GUI and a command line (`dslite`). Flash without opening CCStudio.

**NONMAIN.** A small, special region of MSPM0 flash that holds the chip's boot and security settings. If a flash operation touches it and gets interrupted, the chip can be locked forever. CCStudio's flash loader has guards for this; raw tools may not..

**BSL.** Bootstrap loader. A fallback way to program the chip over UART or I2C without a debugger, entered by holding a button at reset.

**Zephyr.** An open-source real-time operating system for small chips, a Linux Foundation project. One build tool (`west`) and one driver model across chips from many vendors, so code written for one board can move to another. Upstream Zephyr lists some TI boards (for example the LP-MSPM0G3507 and LP-EM-CC2340R5); each board page says whether its board is listed.

**west.** Zephyr's command-line tool. `west build -b lp_mspm0g3507 samples/basic/blinky` then `west flash`.

**RTOS.** Real-time operating system. Lets a chip run several tasks with timing guarantees. FreeRTOS and Zephyr are the two you will meet. "nortos" in a TI example path means bare metal, no OS.

**Bare metal.** Your program and nothing else running on the chip. What the blink example is.

**MCP.** Model Context Protocol. The standard way an AI agent calls tools. TI ships four MCP servers inside CCStudio: build the project, run the debugger, edit SysConfig, read the serial port. They are what turn "the agent wrote code" into "the agent flashed the board and watched it run."

**Agent.** The AI coding assistant acting on its own through tools, as opposed to a chatbot that only answers. Claude Code and Codex are the two TI supports inside CCStudio.

**UART / serial / backchannel.** The chip's text output, carried over the same USB cable through the XDS110, shown in a terminal at a baud rate such as 115200. How a program says "hello."

**Baud rate.** Speed of the serial link. Both ends must agree. 115200 is the usual default. Mbaud is a million bits a second; kbaud a thousand.

**Export approval.** The one-minute form TI shows before most downloads, asking where you are and what the software is for. US export law, applied per download.

**myTI.** The ti.com account. Required for downloads and the store.

**EVM.** Evaluation module. TI's general word for any board you can buy to try a chip. A LaunchPad is one kind of EVM.

**Rosetta.** Apple's translator that lets Intel-only Mac software run on Apple Silicon Macs. Needed for CCStudio on Apple Silicon. Not needed on an Intel Mac.

**Electron**: the framework that lets a desktop app be built from web technology. VS Code and CCStudio 21 are both built on it. `ELECTRON_RUN_AS_NODE` is a setting that tells an Electron app to skip its window and act as a plain Node.js runtime.

**AGENTS.md.** A plain text file of notes that AI coding assistants (Codex, Cursor, GitHub Copilot) read when they start work in a folder; Claude Code reads the same notes from a file named `CLAUDE.md`. Each board folder here has one: what the runs on that board showed, every line from a run. Copy it into your own project.

**llms.txt.** A short index file at the root of a site or repository, one line per page with what it answers, that AI assistants read first. `llms-full.txt` is every reader page concatenated into one file for assistants that take one long document.

## Wireless boards and radios (CC23xx, CC27xx)

**kB (KB).** 1 kB = 1,024 bytes in every memory figure on this site; older pages write KB for the same thing. So 36 kB is 36,864 bytes.

**CC2340R5, CC2340R52, CC2340R53.** The LP-EM-CC2340R5 board carries the CC2340R52 chip: 512 kB flash, 36 kB RAM, Cortex-M0+ at 48 MHz. The CC2340R53 is the same family with 64 kB of RAM, sold on its own board, the LP-EM-CC2340R53. TI's product page covers both, which is why it says "up to 64KB".

**CC27xx (CC2755P10).** TI's larger wireless family: Cortex-M33 at 96 MHz, 1 MB flash, 162 KB RAM on the CC2755P10.

**F3 SDK.** The SimpleLink Low Power F3 SDK: TI's software kit for the CC23xx and CC27xx chips (examples, drivers, Bluetooth and Zigbee stacks). Separate from the MSPM0 SDK.

**802.15.4.** The low-power radio standard under Zigbee and Thread. Bluetooth is a different radio standard on the same 2.4 GHz band.

**Zigbee coordinator, router, end device.** The coordinator starts and runs the network (in a home, usually a USB stick on the hub). A router is always on and relays messages. An end device talks only to its parent and can sleep; a sleeping one wakes every few seconds to collect messages.

**ZHA.** Zigbee Home Automation, Home Assistant's built-in Zigbee integration. It uses the zigpy library.

**ZBOSS.** The Zigbee stack, developed by DSR, that TI's software kit for the CC2340 and CC2755 chips uses (TI's Zigbee overview page: "CC2340 and CC2755 support Zigbee through ZBOSS"). It ships as prebuilt libraries.

**ZBT-2.** Nabu Casa's Home Assistant Connect ZBT-2, a USB Zigbee and Thread radio for Home Assistant.

**LQI and RSSI.** Two measures of signal quality the coordinator reports. RSSI is received signal strength in dBm (closer to 0 is stronger; -85 dBm is weak, -91 dBm is near the edge). LQI is a 0 to 255 link-quality score (higher is better).

**OTA.** Over-the-air update: sending new firmware to a device by radio instead of a cable.

**MCUboot.** A small open-source bootloader that sits at the start of flash and installs a new firmware image after an OTA download (it copies or swaps the image, depending on how it was built).

**Thread.** A low-power mesh network for smart-home devices, built on 802.15.4 and IP. Matter devices often use it.

**RCP, MTD, FTD, NCP.** Thread roles. An RCP (radio co-processor) is only the radio; a bigger computer runs Thread and drives it. An MTD (minimal Thread device) is an end device. An FTD (full Thread device) can route. An NCP (network co-processor) runs Thread itself and is controlled by a host.

**Spinel, HDLC.** Spinel is the command language between an RCP and the computer that drives it (set the channel, send this frame, here is a received frame). HDLC is the framing that carries spinel over a UART: a flag byte marks each frame and a checksum closes it.

**Leader, router, child.** Roles inside one Thread network. The leader keeps the network's shared data; routers forward for others; a child hangs off one router. A device that forms a new network becomes its leader.

**Matter.** The smart-home standard backed by Apple, Google, Amazon and others. Runs over Thread, Wi-Fi or Ethernet; Bluetooth LE is used only to set a device up.

**DMM.** Dynamic Multi-protocol Manager: TI's software that lets one radio run Bluetooth and an 802.15.4 stack (Zigbee, Thread or TI 15.4) at the same time by taking turns.

**CCFG.** Customer configuration: a small area of flash at 0x4E020000 on CC23xx chips that tells the boot code how to start, which debug access is allowed, and whether the chip can be erased. It carries checksums (CRCs) the boot code checks. A wrong CCFG can stop a program from running or lock the chip.

**CRC.** A checksum: a number computed from a block of data so a change can be detected. The CC23xx boot code checks CRCs in the CCFG area before running a program; tools fill them in after the build.

**NVS.** Non-volatile storage: an area of flash a program uses to keep settings across power cycles, such as the Zigbee network it joined.

**Heap and .bss.** Two parts of RAM. The heap is a pool a program borrows memory from while it runs; a radio stack needs a large one. `.bss` is the RAM reserved at build time for variables that start at zero. When the linker says ".bss will not fit", the program needs more RAM than the chip has.

**Linker.** The last build step: it places code in flash and variables in RAM. If they do not fit, the build fails here, before anything is flashed.

**CoreMark.** A standard benchmark for small processors. Higher is faster; "per MHz" divides out the clock speed so chips at different speeds compare.

**SysTick, NVIC, VIMS.** Parts of the chip used in the benchmarks. SysTick is the Arm core's built-in timer. The NVIC is the interrupt controller. VIMS is the CC23xx flash cache.

**BTHome.** An open format for Bluetooth sensors to broadcast readings that Home Assistant understands without pairing.

**tilogger.** TI's tool that turns the binary log some SDK examples send over the serial port back into readable text.

**Cluster and endpoint (Zigbee).** A Zigbee device lists numbered endpoints, and each endpoint lists clusters: standard feature blocks such as On/Off (0x0006) or OTA Upgrade (0x0019). A hub only talks to clusters a device lists.

**Poll interval.** How often a sleeping Zigbee end device wakes to ask its parent for waiting messages. TI's examples use 3 seconds.

**Border router.** A device that connects a Thread network to a home's Wi-Fi or Ethernet network, such as an Apple TV or a Home Assistant hub with a Thread radio.

**TI 15.4.** TI's own simple star network on the 802.15.4 radio, used in TI's sensor examples. Not Zigbee or Thread.

**Kconfig.** Zephyr's settings system: options such as `CONFIG_BT=y` in `prj.conf` files turn features on and off at build time.

**objcopy.** A build tool that converts a built program into the file formats a flasher loads (`.hex`, `.bin`).

**lief.** A Python library for reading and editing built program files. TI's checksum tool for its Zephyr fork uses it.

**Flash wait state, wait states.** An extra clock cycle the core waits for each read from flash when it runs faster than the flash can answer. The MSPM0L1306 adds one above 24 MHz; the MSPM0G3507 adds two at 80 MHz. It is why per-MHz scores drop at the top clock.

**SYSOSC.** The MSPM0's built-in oscillator: 32 MHz (or 4 MHz) with no crystal. On the MSPM0L1306 it is the only fast clock.

**Stack.** The RAM a program uses for function calls and local variables; it grows down from the top of RAM. **Stack overflow:** it grows past the space set aside and overwrites other data; on a chip with no memory protection nothing stops it. **Stack painting:** filling RAM with a known pattern before the program runs, then reading back how much was overwritten.

**DMA.** Direct memory access: a block that copies data (for example ADC samples to RAM) without the CPU.

**HardFault and breakpoint.** A HardFault is the Cortex-M's catch-all error. A breakpoint (`__BKPT`) stops the core for a debugger; with no debugger attached, a Cortex-M0+ turns it into a HardFault.

**IQmath.** TI's fixed-point maths library. On MSPM0 chips with a math accelerator it can use the hardware (`MATHACL` version); on others it runs in software (`RTS` version).

**Embench.** Embench-IoT, an open benchmark suite of 19 small real-world programs, meant to be fairer than one number like CoreMark.

**Devicetree and pin-mux (pinctrl).** Zephyr's description of a board's hardware, in text files. The pin-mux part says which chip pin carries which signal (UART TX on PA8, for example).

**MessagePack.** A compact binary version of JSON. TI's out-of-box demos use it to talk to their GUI over the serial port.

**Op-amp (OPA) and GPAMP.** Amplifiers inside the chip. On the LP-MSPM0L1306 one of them turns the photodiode's tiny current into a voltage the ADC can read.

**EnergyTrace.** TI's way of measuring a board's current through its debug probe (the XDS110-ET on many LaunchPad kits, such as the LP-MSPM0L1306 and LP-MSPM0G3507, and the LP-XDS110ET clip-on probe). CCStudio supports it on Windows and Linux; on macOS the IDE switches it off (experiment E11 in `docs/experiments/`). EnergyTrace++ adds which part of the chip was awake; Core Trace, a different feature behind the same menu, records which instructions ran.

**Debug probe.** The chip or box between your computer's USB port and the MCU's debug pins. On a LaunchPad it is the XDS110; on a bare board it is a separate device such as the LP-XDS110ET.

**dylib and rpath.** A dylib is a macOS shared library (Linux calls it `.so`, Windows `.dll`). The rpath is the list of folders the loader searches for the libraries a dylib depends on. `otool -L` lists the dependencies; `otool -l` shows the rpath entries. On macOS a relative entry starts with `@loader_path`; the Linux form `$ORIGIN` means nothing to macOS.

**Sub-1 GHz.** Radio bands below 1 GHz (868 MHz in Europe, 915 MHz in the US). At the same power they reach farther than 2.4 GHz and lose less going through walls, at a lower data rate. Phones do not have this radio, so you need a second Sub-1 GHz radio to receive it.

**Mesh.** A network where devices pass messages on for each other, so a message can hop around the house. In Zigbee and Thread the mains-powered devices do the relaying; battery devices mostly sleep.

**IPv6.** The newer form of internet address. Thread devices always get one, and Wi-Fi devices get one when the home network supports it, so other devices can reach them directly; Zigbee and Bluetooth LE devices do not, and a hub translates for them.

**MQTT.** A simple message format many devices use to send readings to a server. Home Assistant can find MQTT devices on its own (MQTT discovery).

**Radio stack (protocol stack).** The software that runs a radio standard such as Bluetooth or Zigbee: it packs, sends, receives and checks messages. On small chips it is most of the program. Not the same as the stack in RAM.

**Upstream and fork.** Upstream is the main version of an open-source project (for Zephyr, the zephyrproject-rtos repository). A fork is a company's own copy with its changes, such as TI's simplelink-zephyr.

**ESPHome.** A tool that turns a short YAML file into firmware for a small board and connects it to Home Assistant. It also turns ESP32 boards into Bluetooth proxies that relay nearby Bluetooth sensors.

**Link (build step).** The last step of a build, when all the pieces are put together into one image. "Links" means the program builds completely and fits the chip's memory; it does not mean it has run.

**GCC.** The free, open-source C compiler most Arm projects use, and the one Zephyr uses. TI's own compiler, TI Arm Clang, made faster code in this repo's speed tests.

## Porting STM32 code and the Arduino layer (LP-MSPM0G3507)

**STM32, STM32CubeG0, NUCLEO-G071RB.** STM32 is ST's family of Arm microcontrollers. STM32CubeG0 is ST's package of drivers and example projects for the G0 chips. The NUCLEO-G071RB is ST's low-cost board for the STM32G071RB chip. The porting pages here start from its examples.

**HAL and LL.** ST's two C libraries for STM32 chips. HAL (hardware abstraction layer) is the higher-level one, with calls such as `HAL_GPIO_TogglePin`. LL (low layer) is thinner and maps almost one to one onto the chip's registers. On MSPM0 the matching library is DriverLib.

**EXTI.** ST's external interrupt block: it lets a change on a pin interrupt the program. MSPM0 has no block by that name. Its GPIO block raises the interrupt itself, and one handler per port group (`GROUP1_IRQHandler`) sorts out which pin it was.

**USART.** ST's name for a serial port that can also run in synchronous mode. In asynchronous mode it does the same job as a UART.

**PWM.** Pulse-width modulation: a pin that switches on and off fast, where the share of time spent on (the duty cycle) sets the average power. Used to dim LEDs and drive motors.

**ADC.** Analog-to-digital converter: measures a voltage and returns a number. A 12-bit one returns 0 to 4,095 counts.

**Gain error, offset, effective bits.** Three ways an ADC misses. Gain error: the slope is off (0.34% means a full-scale reading lands 0.34% low). Offset: every reading is shifted by the same amount. Effective bits: how many bits survive the noise; 9.6 effective bits from a 12-bit ADC means the last two or three bits are noise.

**Negative control.** In a wiring check, a pin with no wire on it that is read alongside the wired pins. If it moves too, the test is reading noise, not the wire.

**Level shifter.** A small chip that converts between two voltage levels, such as 3.3 V and 5 V, so two boards can talk without damage. Two 3.3 V boards need none.

**Interrupt latency, ISR.** An ISR (interrupt service routine) is the function the chip jumps to when a pin, a timer or a peripheral signals. Interrupt latency is the cycles from that signal to the ISR's first line.

**Jitter, highest rate, CPU load (control loops).** Jitter: the worst time minus the best time from a trigger to the loop's output, over 1,000 periods. Highest rate: the fastest trigger rate at which the loop misses no period in 1,000 (every interrupt runs and every new output lands inside its period), found by a sweep on the board. CPU load: the CPU time one interrupt takes, hardware entry to return, as a percent of a 100 kHz period (10 µs), so chips at different clocks compare. Method: `docs/methodology.md` section 13.

**Soft float, FPU.** Soft float: floating-point maths done in software because the core has no floating-point unit (the Cortex-M0+ in MSPM0 and CC23xx). An FPU does it in hardware (the C28x, the Cortex-M33 in the CC1354P10). Soft float is 10 to 50 times slower.

**SLEEP0, STOP0, STANDBY0, SHUTDOWN (MSPM0).** The MSPM0 low-power modes, lightest to deepest. SLEEP stops the CPU and keeps every clock; STOP keeps a slow clock; STANDBY keeps RAM and a 32 kHz timer; SHUTDOWN keeps nothing and wakes only from a pin.

**LVGL.** An open-source C graphics library for small screens: buttons, labels, charts, drawn into a buffer and sent to the display.

**BitNetMCU.** An open-source project that trains very small neural networks (weights of 1 to 4 bits) to read handwritten digits on cheap microcontrollers, with published cycle counts for the WCH CH32V002.

**OpenThread, ot-ti.** OpenThread is the open-source Thread stack. ot-ti is TI's repository that adds its radios to it. A Thread "dataset" is the network's name, channel, PAN id and keys; a fresh dataset starts a new network.

**Dead band (UART).** A range of baud rates where a loopback fails although slower and faster rates both work. Seen on the MSPM0G3507 and MSPM0L1306 at 5 to 6 Mbaud; cause not found.

**DAC.** Digital-to-analog converter: the opposite of an ADC, it turns a number into a voltage on a pin. The MSPM0G3507 has a 12-bit one; the MSPM0L1306 has only the 8-bit one inside its comparator.

**Variant (results store).** The short code in `data/results/` that says what made a row different from others of the same test: compiler, flags, OS, clock, build. Two rows compare like for like only when their variants mean the same thing.

**PLL.** Phase-locked loop: a circuit that multiplies a slow clock into a fast one. Both ST and TI examples use one to get 56 MHz from a 16 MHz or 32 MHz source.

**ULPCLK.** The ultra-low-power clock on MSPM0: the bus clock many peripherals (UART, timers) run from, set from the CPU clock by a divider. TI's driver header says it "should always remain under 40MHz".

**SPI and I2C.** Two short serial buses chips use to talk to screens, sensors and memory. SPI uses a clock, two data wires and a chip select. I2C uses a clock and one shared data wire.

**Chip select (CS).** The SPI wire that tells one device on the bus that the bytes are for it. Raising it ends a command.

**Level translator.** A small chip that converts signals between two voltages, such as 3.3 V and 5 V. The display module used here has one between its header and the screen.

**Pull-up, active-low, active-high.** A pull-up resistor holds a pin high until something pulls it low. A button is active-low when pressing it pulls the pin low, and active-high when pressing it drives the pin high. On the LP-MSPM0G3507, S2 is active-low and S1 is active-high.

**Debounce.** Ignoring the rapid on-off flicker a real switch makes for a few milliseconds when pressed.

**Skill (Claude Code).** A folder of instructions that Claude Code loads when asked, to follow a fixed procedure. TI ships one, `ti-ccstudio-migrate-to-ti`, inside CCStudio for moving STM32 projects to MSPM0.

**Delegated run.** A second Claude Code run that a skill starts for one step, with its own instructions and its own log. The skill's main run hands work to it and waits.

**Headless.** Running Claude Code from a command (`claude -p`) with no chat window, so it cannot ask you anything. A permission that would normally ask is treated as denied.

**Shim.** A thin layer of code that makes one software interface look like another. The Arduino layer of this repo for the LP-MSPM0G3507 is a shim: it gives Arduino sketches the calls they expect (`digitalWrite`, `Serial`, `SPI`) on top of TI's DriverLib.

**Arduino sketch and ArduinoCore-API.** A sketch is an Arduino program with a `setup()` function that runs once and a `loop()` function that runs forever. ArduinoCore-API is Arduino's own portable part of the core: `Print`, `String` and the declarations of `pinMode` and `delay`. A board maker supplies the rest.

**HSI.** ST's high-speed internal oscillator, a 16 MHz clock inside STM32G0 chips. ST's examples feed it through the PLL to reach 56 MHz. The closest MSPM0 counterpart is SYSOSC (32 MHz).

**SYSPLL.** The MSPM0's own PLL. In the ported projects it multiplies SYSOSC up to a 112 MHz internal clock, which a divider brings to the 56 MHz CPU clock.

**BSP.** Board support package: ST's folder of code that names a board's LED and button pins (`Drivers/BSP/STM32G0xx_Nucleo/` for the NUCLEO-G071RB). Without it, TI's migration skill cannot read the example's pins.

**VREFINT.** ST's internal voltage reference, about 1.2 V, which an STM32 ADC can read as a channel. The MSPM0G3507 ADC has no channel by that name. TI's migration skill substitutes the supply-monitor channel, which reads the supply voltage divided by 3.

**PINCM.** The per-pin control registers of the MSPM0 IOMUX, one per pin (`PINCM19` and so on). Each holds which function the pin carries (plain GPIO, timer output, UART) and its pull-up or pull-down setting.

**TIMA0.** One of the MSPM0G3507's timer blocks, an advanced timer with four compare channels. Port 05 uses it for four PWM outputs.

**LOAD and CCACT.** Two timer registers. `LOAD` holds the number the timer counts down from, which sets the PWM period. `CCACT` (capture and compare action) sets what an output pin does at each event, such as going high at reload and low at the compare match.

**CTL0 and MEMRES.** Two ADC registers. `CTL0` is control register 0, which holds the bit that enables conversions, so 0 means the ADC is not enabled. `MEMRES0` to `MEMRES2` hold the results of each conversion slot, and can be read over the debugger even when DMA has not copied them.

**tiarmsize.** TI's command-line tool that prints a built program's size: code and constants in flash (`text`), initialized data, and zero-initialized RAM (`bss`).

**Bit-banged.** Driving a serial bus such as I2C by setting and reading two ordinary GPIO pins in software, instead of using the chip's own I2C block. It works on any two pins and runs slower.

**C2000, C28x.** TI's family of control microcontrollers (C2000) and the CPU inside them (C28x). Not Arm, so it uses TI's own C2000 compiler. Its smallest memory unit is 16 bits: a `char` is 16 bits and there is no `int8_t`.

**C2000Ware.** TI's SDK for C2000 chips: drivers, examples, DSP and math libraries. TI's public GitHub copy needs no login.

**TMU.** Trigonometric math unit. Hardware on some C2000 chips that does sine, cosine, atan2, square root and divide in a few cycles.

**cJTAG.** A 2-wire version of the JTAG debug link. The LAUNCHXL-F2800137 uses only this.

**FOC (field-oriented control).** The standard math for driving a brushless motor smoothly: measure two phase currents, rotate them into the rotor's frame, run two controllers, rotate back, set three PWM duty cycles. Runs once per PWM period.

**ePWM.** The C2000's PWM timer module. Besides driving pins it can start ADC conversions at exact points in its cycle.

**SOC (start of conversion).** On C2000 ADCs, one programmed conversion slot: which pin, how long to sample, what starts it. Not "system on chip" here.

**Msps.** Million samples per second, an ADC's conversion rate.

**ESTOP0.** A C28x instruction that halts the CPU when a debugger is attached (and does nothing otherwise). C2000Ware examples use it as an "inspect results here" stop.

**OTP (one-time programmable).** Memory that can be written once and never erased. On the F2800137 it holds the security and boot settings; writing it is permanent.

**DCC (dual-clock comparator).** C2000 hardware that counts one clock against another to check a clock's frequency.

**Projectspec.** CCStudio's XML project template (`.projectspec`) that lists an example's files and build options; CCStudio builds a project from it on import.

**Golden vectors, golden-vector.** Known inputs with known correct outputs, shipped with an example so a run can check its own answers.

**Connection file (.ccxml).** The file that tells TI's debug tools which probe and which chip to use.

**PARTIDH, REVID.** Registers inside a C2000 chip that say which part and which silicon revision it is.

**CPU Timer 1.** One of three 32-bit timers in every C2000 chip; this project uses it to count CPU cycles.

**Reset types.** The debugger's default reset restarts the CPU; a System Reset also resets the clock and peripheral settings, which some examples change.

**.TI.ramfunc.** The section name TI uses for functions copied from flash to RAM at start-up, so they run without flash wait states.

**DMIPS.** Dhrystone MIPS: Dhrystones per second divided by 1,757. DMIPS/MHz compares chips at different clocks.

**DCSM.** Dual code security module, the C2000 feature that locks flash and RAM zones with passwords kept in OTP.

**NNC.** TI's Neural Network Compiler: turns a trained model (ONNX) into C code and a library for a TI chip.

**FPU.** Floating-point unit. The F2800137's FPU32 does single-precision float in hardware; double precision runs in software.

**Twiddle factors.** The sine and cosine table an FFT uses; it takes RAM or flash like the data does.

**xdsdfu.** TI's command-line tool for updating XDS110 probe firmware and listing probes.

**Bitfield examples.** C2000Ware's register-level examples (`device_support/.../examples`) that write registers through C structs instead of DriverLib calls.

**Logits.** A neural net's raw output scores, one per class; the biggest one is the answer.

**NPU, TinyEngine.** A neural processing unit: a block on the chip that runs a neural net's layers faster than the CPU. TinyEngine is TI's name for the one in the MSPM0G5187.

**HDC2080, TMP117, DRV5055, BMI160, BMM150.** The other sensors on TI's BP-BASSENSORSMKII Sensors BoosterPack: humidity and temperature, a precise thermometer on a break-away strip, a magnet sensor with an analog output, a motion sensor (accelerometer and gyroscope), and a compass read through the motion sensor. PIR is a passive infrared motion detector, the kind in a room light switch.

**OPT3001, lux.** The OPT3001 is a TI light sensor read over I2C. Lux is the unit of light as the eye sees it: a dim room is tens, a lit desk a few hundred, daylight tens of thousands.

**q15.** A 16-bit fixed-point number format with 15 fractional bits, used for DSP on chips without an FPU.

**Profile clock.** A cycle counter in the debugger that counts while the CPU runs.

**eQEP.** The C2000's quadrature encoder input, for reading a motor's position sensor.

**STANDBY, HALT, IDLE.** C2000 low-power modes. In STANDBY and HALT the CPU clock stops until a wake-up event; a debugger cannot halt the chip then.

**Early interrupt (C2000 ADC).** An ADC setting that runs your interrupt code at the end of the sample window instead of the end of the conversion. Your code starts about 20 cycles sooner, but the result is not ready, so it must wait for it before using it.

**Prefetch (flash).** Hardware that reads the next flash words before the CPU asks for them, with a small cache that keeps recent ones, so code in flash loses fewer cycles to wait states. TI's C2000 `Device_init()` turns both on.

**End-of-conversion interrupt (C2000 ADC).** The ADC's default setting: your interrupt code runs once the conversion has finished, so the result is ready to read.

**TMU flags.** The two C2000 compiler options `--tmu_support=tmu0` and `--fp_mode=relaxed`, which together make the compiler use the TMU for sine, cosine, atan2, square root and divide.

**Context save.** The registers an interrupt saves on entry so the interrupted code can carry on afterwards; it costs cycles before your interrupt code runs, and the matching restore costs cycles after.

**Optimization level.** How hard the compiler works to make code fast or small: `-O2`, `-O3`, `-O4` (TI's C2000 compiler also writes `--opt_level=4`; level 4 optimizes across files). `--opt_for_speed` from 0 to 5 trades size for speed on top.

**Trampoline (linker).** A small jump stub the linker adds when a call is too far for the instruction; TI's linker names it in its "will not fit" error, but the usual cause is simply too little memory.

**FFT (fast Fourier transform).** Turns a block of samples into the strength of each frequency in it. "1,024 points" is the block size.

**Inference.** One run of a trained neural net on one input, giving one answer.

**CNN.** Convolutional neural net: a neural net that slides small filters across an image, the usual kind for reading pictures.

**Fully connected.** A neural-net layer where every input feeds every output; simple, but it needs many weights.

**MNIST.** The classic set of 70,000 handwritten digits, 0 to 9, used to train and test digit readers.

**Room-occupancy model.** This project's four small neural nets that count the people in a room (0 to 3) from light, sound, CO2 and motion readings, in sizes from 636 to 33k weights. The same models and test vectors run on every board, so their times compare chips.

**ONNX.** An open file format for trained neural nets. TI's NNC reads it.

**int8, quantize.** int8 is an 8-bit whole number from -128 to 127. Quantizing turns decimal numbers (a model's weights, or its input) into int8 so the chip can use small, fast integer math.

**Multiply-add.** One multiplication added to a running total, the basic step of a neural net or a filter. The count of them measures how much work a model is.

## Bluetooth services, Web Bluetooth and porting

**GATT, GATT service, attribute table.** GATT (Generic Attribute Profile) is how a connected Bluetooth device offers values to a phone or computer. A service is a group of related values, such as "LED" or "Health Thermometer". On the device, the services live in an attribute table: a numbered list of entries (the service, each value, each setting), and the phone reads and writes them by their number, called a handle.

**Characteristic.** One value inside a GATT service, such as the LED state or a temperature, with rules for what a phone may do with it: read, write, or be told when it changes.

**UUID (128-bit, 16-bit).** The ID that names a service or characteristic. Standard ones from the Bluetooth SIG are short, 16-bit numbers such as `0x1809` (Health Thermometer); custom ones are 128-bit, written as text like `F0001110-0451-4000-b000-000000000000`. On air the 128-bit form is sent low byte first, the reverse of the text, which is an easy place to get the byte order wrong.

**Notify, notification, indicate.** Ways a device pushes a new value to a connected phone without being asked. A notification is sent once with no reply; an indication waits for the phone to confirm it. The phone has to turn either one on first, through the CCCD.

**CCCD (client characteristic configuration descriptor).** A small setting attached to a characteristic that a phone writes to turn notifications or indications on or off. Each one takes an entry in the attribute table.

**Web Bluetooth.** A browser feature that lets a web page connect to a Bluetooth device after the user clicks and picks it from a list. Chrome and Edge on a computer and Chrome on Android have it; Safari and Firefox do not. It works only on HTTPS pages or on `localhost`.

**Advertising, advertisement, beacon.** A Bluetooth device sends short packets every so often, to anyone listening, saying its name and what it offers; that is advertising. A beacon is a device that only advertises and never takes a connection, putting its readings (a temperature, for example) in the advertisement itself, as BTHome does.

**Project Zero.** TI's Bluetooth example for its CC13xx and CC26xx boards: LED, button and data services a phone can use. It is the program the LP-CC1354P10-1 ships with, and it advertises as "Project Zero".

**Glue (vendor glue).** The short piece of code that connects portable code to one vendor's stack or drivers: where the radio's buffer is, how to start a timer, how to read a sensor. It is the part rewritten for each chip family; the rest stays the same.

**Hex file.** A text file (`.hex`, Intel HEX format) that holds a built program and the flash addresses it goes to. Flashing tools such as UniFlash read it. TI ships ready-made hex files for some examples in the SDK, so you can flash them without building.

## Firmware updates over Bluetooth, and Zigbee device details

**OAD (over-the-air download).** TI's name for sending new firmware to a running Bluetooth device and having it install itself. TI's F3 SDK has four OAD examples for the CC23xx: off-chip, on-chip, dual image and multi-step.

**Dual image (OAD).** An OAD layout where flash holds two slots of the same size: the app runs from the first while the new image downloads into the second, then the bootloader installs it. It halves the room for your app.

**SHA-256, ECDSA signature (image signing).** SHA-256 is a 32-byte fingerprint of a file: change one bit and it changes. An ECDSA signature (here ECDSA P-256) is made from that fingerprint with a private key; the bootloader checks it with the matching public key, so it installs only images signed by whoever holds the private key.

**imgtool.** MCUboot's command-line tool that adds the image header, the SHA-256 and the signature to a built app. TI's OAD examples run it as a post-build step.

**ATT MTU (packet size).** The largest piece of data one Bluetooth write or notification can carry on a connection, agreed by the two sides when they connect. TI's OAD sends 240-byte blocks, so it needs an ATT MTU of 243 or more.

**bleak.** An open-source Python library for Bluetooth LE that works on macOS, Windows and Linux. This project's OAD client and Bluetooth tests use it.

**0xFF padding.** Bytes of 0xFF added to fill a file out to a fixed size, such as a flash slot. Erased flash reads 0xFF, so padding writes nothing new, but it still has to be sent.

**CCFG checksums.** Four CRC32 values inside the CC23xx CCFG that the boot code checks before it trusts the settings. UniFlash recalculates them while flashing by default ("Overwrite CRC fields in CCFG"); an image with them set to zero boots only if the flasher fixes them.

**Node descriptor (Zigbee).** A short record every Zigbee device sends about itself: its type (coordinator, router, end device) and a capability byte. In that byte, bit 3 set means "receiver on when idle"; a sleepy end device has it clear.

**Trust center.** The device in a Zigbee network that hands out the network key, almost always the coordinator.

**NV (Z-Stack).** Non-volatile storage: the flash area TI's Zigbee stack uses to keep the network it joined, its key and its address across resets. The NVS entry above is the same idea in TI's other SDKs.

**Factory new, RESET TO FN.** A Zigbee device is factory new when it holds no saved network. RESET TO FN ("reset to factory new") is the item in TI's Zigbee serial menu that erases the saved network so the device can join again.

**Zephyr SDK.** The bundle of compilers (GCC for Arm and other cores) and host tools that Zephyr builds with, downloaded separately from Zephyr itself: <https://github.com/zephyrproject-rtos/sdk-ng/releases>.

## Splitting a demo into portable code, glue and configuration

**SIG profile.** A standard service defined by the Bluetooth Special Interest Group (the SIG), such as Health Thermometer (`0x1809`). Any phone app that knows the standard can read it, whichever chip sends it.

**Host test.** A test you run on the Mac, not the board: plain C compiled with the Mac's own compiler (`cc`), checking that code gives the expected bytes before it goes near a chip.

**Twin build.** Building the same portable code against two different stacks or vendors (for example TI's SDK and Zephyr) to show that the portable part really does not change between them.

**GPIO.** General-purpose input and output: an ordinary pin the program can set high or low (to light an LED) or read (to see a button).

**Makefile.** The text file the `make` tool reads to know which files to compile and how. TI's command-line examples ship one per project; a port may add a line or two to it to build an extra file.

**TI Drivers.** TI's higher-level driver library (`UART_open`, `ADC_open`, `GPIO_setConfig`). It sits on top of DriverLib and can run with FreeRTOS or with no RTOS. On MSPM0 its configuration is hand-written C tables, not SysConfig.

## Code written for 8-bit bytes on a 16-bit-char chip (C28x)

**u8g2, u8x8.** An open-source C library for small monochrome screens (OLED and LCD). u8x8 is its text-only half that writes 8x8 characters straight to the screen; u8g2 draws text, lines and shapes into a frame buffer in RAM and sends that.

**SSD1306.** A common controller chip on 128x64 and 128x32 monochrome OLED screens, driven over I2C or SPI.

**Packed struct.** A C structure the compiler lays out with no padding bytes between fields, so it can be copied straight onto a wire or into a file. GCC and Arm compilers spell it `__attribute__((packed))`; the C2000 compiler rejects that attribute.

**Bit field.** A struct member with a stated width in bits (`uint8_t req : 7;`). How bit fields are packed depends on the compiler and the size of the type around them.

**Byte order, little-endian.** The order a multi-byte number is stored in memory. Little-endian puts the lowest byte first. Arm Cortex-M chips and the C28x are both little-endian.

**Transceiver, BP-CC1407P.** A transceiver here is a radio chip that does not run your application; a host microcontroller sends it commands. The BP-CC1407P is TI's BoosterPack with the CC1407P sub-1 GHz transceiver, driven over SPI.

**SCFG.** Security configuration: a 1 kB flash area at 0x4E040000 on CC27xx chips, next to the CCFG, that holds the secure boot, debug-key and HSM-update settings. The chip opens its debug port only when both CCFG and SCFG are valid, so a CC27xx backup and restore must include it.

**HSM.** Hardware security module: a separate security processor inside CC27xx chips that does the crypto (AES, SHA-2, ECC, the random number generator). It runs its own firmware from the top 96 kB of flash, which the debugger cannot read. TI's SDK ships that firmware in `bin/hsm/`; Bluetooth and Thread stacks need it.

**SACI.** The device management interface in CC23xx and CC27xx boot code. A debug probe talks to it through the SEC-AP to erase, program and verify flash and to get debug access, without the CPU running. TI's flash tools use it behind the scenes.

**AHB-AP, CFG-AP, SEC-AP.** Three doors the SWD debug connection can open on CC23xx and CC27xx chips. AHB-AP reaches the CPU and memory (normal debugging, closed unless the configuration allows it). CFG-AP reads chip ID and boot status (always open). SEC-AP carries SACI commands.

**Boot status (BOOTSTA).** One byte the chip's boot code updates as it starts, readable through the CFG-AP even when debugging is closed. 0x20 means waiting in SACI, 0xC2 means the application started; the full list is in the chip's technical reference manual.

**Chip erase.** A SACI command that erases main flash and the configuration areas in one go. On CC23xx it is the way back from a CCFG that closed the debug port; on CC27xx it also closes the debug port until a full image with CCFG and SCFG is programmed again (per TI's technical reference manual).

**GEL.** TI's scripting language for the debugger (General Extension Language). CCStudio runs `.gel` files from `ccs_base/emulation/gel/` when it connects, and they also hold menu scripts such as Chip Erase.

**xds110reset.** A command-line tool in CCStudio (`ccs_base/common/uscif/xds110/`) that pulls the target's reset line through an XDS110 probe.

**uhubctl.** An open-source command that turns power on and off for one port of a USB hub that supports per-port power switching. Used here to power-cycle a debug probe and its board without unplugging it; install with `brew install uhubctl`. Not every hub really cuts power: [docs/remote-board-recovery.md](remote-board-recovery.md).

**Secure Debug Manager.** The part of CCStudio and UniFlash that asks a CC27xx chip for a debug key before connecting to its CPU (`DebugServer/bin/sdm/`). Its errors start with "Unable to authenticate with Secure Debug Manager".

**ROM serial bootloader.** A small programmer built into the chip's ROM that loads flash over UART or SPI. A CC27xx with no valid configuration starts it by itself (boot status 0xBA).

**Connect under reset.** A debugger setting that holds the chip in reset while the probe attaches, so the program in flash never runs first.

## Linux vision boards (SK-TDA4VM)

**C7x.** The TDA4VM's digital signal processor core (DSP). TI's deep-learning models run on it, with the MMA beside it doing the heavy arithmetic.

**MMA (matrix multiply accelerator).** The block next to the C7x that does the multiply-add arithmetic of a neural network. TI rates the pair at 8 TOPS (trillion operations a second, 8-bit).

**TIDL.** TI Deep Learning: TI's software that compiles a model for the C7x and MMA and runs it there from ONNX Runtime, TFLite or DLR, or from a GStreamer element.

**ONNX Runtime, TFLite, TVM and DLR.** Four ways of running a trained model: ONNX Runtime and TFLite (TensorFlow Lite) read a model file at run time; TVM compiles the model ahead of time and DLR is the small runtime that loads what TVM produced. TI's model zoo ships models for all three paths.

**fp32 and int8.** The number format a model's arithmetic uses: 32-bit floating point (what training produces) or 8-bit integers (what accelerators run; the model is converted, or quantized, first).

**Model zoo.** TI's folder of models already compiled for the board (`/opt/model_zoo`), each with its artifacts and a `param.yaml` that names its input shape and type.

**GStreamer.** The Linux media framework TI's vision demos are built on: a pipeline of elements (a file source, a decoder, a scaler, the model, a sink) connected in a line.

**NV12.** The pixel layout the hardware decoder and scaler use: one plane of brightness, then the color at half resolution; 1.5 bytes per pixel.

**Zero copy.** Passing a frame between hardware blocks by handing over the buffer, not copying its bytes. In TI's pipelines `capture-io-mode=5` (the decoder imports buffers, DMABUF) and `tiovxmemalloc` (allocates buffers the other blocks can share) do this.

**VPAC.** The TDA4VM's vision block that scales and converts images in hardware, which `tiovxmultiscaler` and `tiovxdlpreproc` use.

**SA2UL.** The TDA4VM's security accelerator: hardware AES, SHA and a random number generator, driven by a Linux kernel driver.

**AF_ALG and /dev/crypto.** Two ways a Linux program can ask the kernel to do cryptography: a socket type (AF_ALG) and a device file from the cryptodev module. Which ones work depends on the kernel's configuration.

**U-Boot.** The bootloader that runs before Linux on these boards. **UART boot** is the chip's built-in way of taking that bootloader over the serial port when no card holds one; the **boot ROM** is the fixed program in the chip that does it.

**perf.** The Linux profiling tool; `perf stat` counts the CPU's cycles and instructions for one program run.

**DMIPS.** Dhrystone's unit: Dhrystones a second divided by 1,757, the score of the 1980s reference machine. DMIPS per MHz removes the clock.

**Cortex-A72.** The two Arm application cores on the TDA4VM that run Linux, at 2.0 GHz here.

**Edge AI SDK (Edge AI image).** TI's Linux build for its vision boards: the kernel, the TIDL runtimes, the model zoo and the GStreamer demos on one micro-SD card image.

**Execution provider.** ONNX Runtime's word for where a model runs: its CPU provider on the Arm cores, or TI's TIDL provider on the C7x.

**Zoo model names.** TI's model zoo names start with the runtime (ONR for ONNX Runtime, TFL for TFLite, TVM for the TVM and DLR path), then the task (CL classification, OD object detection, SS segmentation, KD keypoints), a number, and the network: YOLOX (a detector), regNetx and MobileNet (small classifiers), SSD-lite (a detector) and DeepLab (segmentation).

**maxrss.** The most memory a process held at once, as `/usr/bin/time` reports it, in kB.

**ioctl.** A system call a program uses to hand a request to a device driver, here the way `/dev/crypto` takes a block to encrypt.

**Crypto driver priority.** Each Linux crypto driver registers a number; when two drivers offer the same algorithm the kernel uses the one with the highest number, so the SA2UL's 30,000 beats the CPU's 300.

**IPsec, dm-crypt, authenc.** Two kernel users of cryptography (encrypted network packets, encrypted disks) and the kernel's name for one operation that encrypts and authenticates together.

**fakesink and fpsdisplaysink.** GStreamer elements that end a pipeline: `fakesink` throws the frames away (so nothing paces the pipeline), `fpsdisplaysink` counts them and reports frames a second.

**xmodem and ymodem.** Two old serial file-transfer protocols; the TDA4VM's boot ROM and its early bootloader take their files this way during UART boot.

**DMABUF.** Linux's way of sharing one memory buffer between devices, so a decoder's output can be another block's input with no copy.

**TIOVX.** TI's layer over the OpenVX vision standard; the `tiovx*` GStreamer elements (`tiovxmultiscaler`, `tiovxdlpreproc`, `tiovxmemalloc`) and `tidlinferer` run their work through it on the hardware blocks.

**XIP (execute in place).** Running a program's code straight out of the external flash chip instead of copying it to RAM first. It frees RAM and costs speed, because flash is slower than RAM.

**Cache (L1, L2).** A small fast memory that keeps copies of recently used code or data. The L1 cache is built into the processor core; an L2 cache is a second, larger one between the core and slower memory. On the AM263P, TI's OptiFlash L2 cache (8 to 64 kB) sits in front of the external flash.

**OptiFlash.** TI's name for the AM263P's hardware that makes XIP faster: the L2 flash cache and the tools around it, including smart placement.

**Smart placement.** A TI build step that measures which functions run most, then places those in on-chip RAM and leaves the rest in flash.

**PHY tuning (flash).** A calibration the flash driver runs at start-up so the chip can read the external flash at full speed. It reads a known 128-byte pattern stored in the flash; if the pattern does not read back correctly, the driver falls back to a slower clock.

**controlCARD.** A TI development board format: a small card with the chip that plugs into a larger docking board. TI's AM263P OptiFlash examples were written for the controlCARD, not the LaunchPad.

**DSLite.** The command-line loader that ships with CCStudio. It connects to the board's debug probe and loads a program into RAM or flash, with no IDE open.

**Development boot.** Loading a program into RAM over the debug probe while the chip waits, instead of the chip booting a program from its flash.

**Golden values (golden vectors).** The outputs a test should produce, stored with the test. A run passes when its outputs match them.

**IPC (inter-processor communication).** Ways for the cores of one chip to pass messages. On the AM263P, TI's IPC Notify sends small numbers between cores through mailbox hardware and interrupts.

**ThreadX.** A small real-time operating system (now Eclipse ThreadX), an alternative to FreeRTOS that TI's AM263P SDK also supports.

**Cortex-R5F and R5FSS.** The AM263P's processor cores: Arm Cortex-R5F, built for real-time control. They come in clusters of two (R5FSS0 and R5FSS1); R5FSS0-1 is the second core of the first cluster.

**Lockstep.** Two cores running the same instructions in step, with hardware comparing them, so a fault in one is caught. Safety designs use it; it halves the number of usable cores.

**Split mode (dual-core mode).** The opposite of lockstep: each core of a cluster runs its own program. TI's documents call it dual-core mode.

**TCM (tightly coupled memory).** A small RAM wired straight to one core, as fast as its cache and with no cache misses. Each AM263P R5F core has two banks, TCM-A and TCM-B; TI gives 32 kB each in split mode and 64 kB in lockstep.

**Out-of-tree.** Board or chip support kept in your own folder instead of inside the Zephyr source, and pointed to at build time.

## Sub-1 GHz radio protocols (CC1407P)

**PHY.** The physical layer of a radio link: how bits become a signal, plus the preamble (a known pattern the receiver locks onto), the sync word (the bit pattern that marks where the packet starts) and the header around each packet. Whitening scrambles the payload with a known sequence so long runs of ones or zeros do not upset the receiver. A "PHY profile" is one complete set of these settings.

**2-GFSK.** Two-level Gaussian frequency shift keying: each bit shifts the carrier up or down in frequency, with the edges smoothed. The simplest sub-1 GHz modulation, and the one the weather station uses at 50 kbps.

**802.15.4g.** The IEEE standard for smart-utility radio networks below 1 GHz. It defines the FSK, OFDM and O-QPSK PHYs and a 2-byte packet header (PHR) carrying the length and whether the payload is whitened.

**Wi-SUN.** An industry profile built on 802.15.4g, used for utility meters and street lights. Its FSK "operating modes" are numbered: mode 2b is 100 kbps and mode 4b is 200 kbps.

**OFDM.** Orthogonal frequency division multiplexing: the data is split across many closely spaced carriers sent at once. Wi-SUN OFDM has four "options" (option 1 is the widest channel, option 4 the narrowest) and several MCS levels.

**MCS.** Modulation and coding scheme: within one OFDM option, a higher MCS packs more bits per symbol and adds less error correction, so it is faster and needs a stronger signal.

**O-QPSK.** Offset quadrature phase shift keying, a phase modulation 802.15.4 uses. SUN O-QPSK is its sub-1 GHz form (SUN: smart utility network, the 802.15.4g name). Its "rate mode" picks the data rate at a fixed 100 kchip/s: SysConfig's CC1354P10 settings name rate modes 0 to 3 as 6.25, 12.5, 25 and 50 kbps (F2 SDK 8.33 radio settings, read 2026-10-11).

**DSSS.** Direct sequence spread spectrum: each bit is sent as several chips, which slows the data rate but lets the receiver pull the signal out of more noise. The number after DSSS in TI's profile names is its spreading setting; in the measured rates, a higher one is slower.

**OFDM-LR and XLR.** TI's long-range modes for the CC1407P: "SimpleLink OFDM Long-Range" and "Extended Long Range" (20 ksps, 10 kHz deviation), as TI's settings headers name them. They trade data rate, down to 0.69 kbps, for range.

**Wi-SUN FSK modes 1a to 5.** The numbered FSK settings of Wi-SUN: 1a and 1b are 50 kbps, 2a and 2b 100 kbps, 3 is 150 kbps, 4a and 4b 200 kbps, 5 is 300 kbps (SysConfig's CC1354P10 setting names). The letters differ in deviation, how far the frequency swings for each bit.

**SimpleLink Long Range.** TI's long-range mode for its CC13xx wireless MCUs: 20 ksps with spreading and error correction, giving 5 or 2.5 kbps.

**RSSI and dBm.** RSSI is the received signal strength a radio reports for a packet or a channel, in dBm: decibels relative to one milliwatt, so -20 dBm is strong and -100 dBm is near the noise.

**Carrier sense (listen before talk).** The radio measures the channel and reports idle or busy against a threshold, so a sender can wait for a clear channel.

**Command chaining.** On the CC1407P, a stored radio command can name the next one to run when it ends, so the radio switches from transmit to receive by itself without waiting for the host.

**Ping (in the radio tests here).** A short packet, `FRL PING <n>`, which the other radio answers with `FRL PONG <n> r=<signal it was heard at>`, so one exchange measures the round trip and the signal at both ends.

**MCU-Link.** NXP's debug probe built into its development boards; it speaks the CMSIS-DAP protocol over USB and also provides a serial port.

**pyOCD.** An open-source flasher and debugger for Arm chips; it needs a device pack for a chip it does not know (for the FRDM-MCXA153, `pyocd pack install MCXA153VLH`).

**LPCAC.** The small instruction cache in front of flash on NXP MCX A chips; it hides flash wait states.

**FRO.** The chip's internal oscillator (free-running oscillator); the MCXA153 runs at 96 MHz from it with no crystal.

## Wake words and the onboard microphone (LP-MSPM0G5187)

**Wake word.** A short phrase a device listens for all the time ("OK Kilby" in TI's example). Hearing it is the cue to act, here lighting an LED and printing a line.

**I2S.** A three-wire digital audio link: a bit clock, a word clock that marks each sample, and a data line. On the LP-MSPM0G5187 the chip makes both clocks and the board's audio converter sends the samples back on the data line.

**PCM1809.** The audio converter on the LP-MSPM0G5187. The board's analog microphone (an ICS-40720) feeds it, and it sends digital samples to the chip over I2S.

**Decimation.** Lowering the sample rate by keeping one sample in N after a low-pass filter that removes what would otherwise fold down into the kept band. The rate goes from 47,619 to 7,936.5 samples a second, one in six.

**LPC (linear predictive coding).** A way to describe a short slice of speech by the shape of its spectrum. TI's example turns each 20 ms of audio into 70 numbers this way and hands the model the last 100 of them, two seconds.

**dBFS.** Decibels below the largest value a sample can hold. A peak of -20 dBFS means the loudest sample is one tenth of full scale.

**False trigger.** The device reports the wake word when it was not said.

**Replay (this repo's test input).** The Mac sends recorded or synthesized speech to the board's serial port instead of the microphone, so the same model code sees the same audio on every run.

## Reading a running chip (live chip view)

**Debug port.** The wires a debugger uses to look inside a chip. On the LaunchPad kits the on-board XDS110 probe talks to it over USB. A debug port can read memory and registers while the CPU runs, without stopping it, as long as you do not connect to the core itself.

**WebHID.** A Chrome feature that lets a web page talk to a USB device that shows up as a human interface device. The live chip view uses it to reach the XDS110 probe from the browser. Safari and Firefox do not have it.

**DHCSR.** The Arm debug halting control and status register. One bit says whether the CPU is asleep (waiting for an interrupt), which gives the awake share.

**DWT_PCSR.** The Arm register that holds a sample of the program counter, the address of the instruction the CPU is on. Reading it many times a second shows where the CPU spends its time. The Cortex-R5 cores of the AM243 do not have it.

**ICSR.** The Arm interrupt control and state register. Its low bits give the number of the interrupt handler that is running now.

**NVIC_ISER.** The Arm register whose bits say which interrupts are turned on (the interrupt controller is called the NVIC).

**PWREN.** On an MSPM0 chip, the power enable register of a peripheral such as a UART or timer. A peripheral that is off cannot be used, so this register shows which blocks the program turned on.

**PINCM.** On an MSPM0 chip, the pin control register of one pin. It says which function the pin has (GPIO, UART, timer output), and its pull-ups and input enable.

**DOE, DOUT, DIN.** The three MSPM0 GPIO port registers: DOE says which pins are outputs, DOUT is the level the program drives, DIN is the level on the pins.

**PSC.** The AM243's power and sleep controller. Each module has a switch (an LPSC number), and its status register says whether the module has power and a clock.

**OCRAM.** On-chip RAM shared by the AM243's cores, reached through the data cache of an R5F core. A variable read from OCRAM through the debug port can look frozen because the core still holds the new value in its cache.

**WFI.** The Arm instruction "wait for interrupt": the CPU stops until an interrupt arrives. A program that sleeps with WFI shows as asleep in the debug port.

**PWM.** A square wave with a set on-time, used to dim an LED or drive a motor.

## Power supplies

**GaN.** Gallium nitride: a transistor material that switches faster and wastes less power than silicon. TI's LMG708B0 has GaN switches built in.

**ESR.** Equivalent series resistance: the small resistance inside a capacitor. Ripple current through it adds ripple voltage, and it adds a zero to a regulator's loop.

**Loop gain.** How much a small disturbance grows going once around a regulator's feedback loop. Where it is above 1 (0 dB), the regulator corrects errors.

**Crossover.** The frequency where the loop gain falls to 1 (0 dB). Roughly, how fast the regulator reacts.

**Phase margin.** How far the loop's phase is from −180° at crossover. At 0° the loop oscillates; 45° to 60° is the usual target.

**Bode plot.** Gain and phase against frequency on a log axis.

**Network analyzer.** An instrument that sweeps a sine through a circuit and measures gain and phase at each frequency. Used with an injection transformer to measure a regulator's loop while it runs.

**Devicetree overlay.** A small text file that adds to or changes a board's Zephyr hardware description without editing the board's own files. In the Arduino-on-Zephyr folder it maps Arduino pin numbers to chip pins.

**Static module / LLEXT.** Two ways to add code to a Zephyr image. A static module (`-DZEPHYR_EXTRA_MODULES`) is compiled into the image with everything else. LLEXT (loadable extensions) loads a separate piece of code at run time. The Arduino folder uses the first.

**nRST.** The chip's reset pin. Pulsing it low restarts the chip. After a DSLite flash the LP-MSPM0G3519 only runs a Zephyr image once nRST has been pulsed.

**Tinning.** Putting a thin layer of solder on a pad or on the stripped end of a wire before you join them.

**Cold joint.** A solder joint that did not get hot enough: it looks dull or like a ball sitting on the pad, and can fail to conduct. Reheat it.

**Continuity.** A meter setting that beeps when two points are joined by a wire or a solder joint. Used to find shorts and broken wires.

**Heat-shrink.** A plastic tube that shrinks when warmed. Slid over a joint before heating, it covers the bare solder.

**Lever nut.** A small connector with levers that clamps two to five wires together without screws or solder (for example the WAGO 221 series).

**Cable gland.** A fitting in a box wall that grips a cable and seals the hole around it. A drip loop is the dip a cable makes below the gland before it runs on, so rain drips off the low point.

**GFCI.** A ground-fault circuit interrupter: an outlet that cuts power when it senses current leaking where it should not go. Recommended for outdoor outlets.

**Level shifter.** A chip that lifts a 3.3 V signal to 5 V (or back). The holiday lights builds use a 74AHCT125 so a 3.3 V board can drive 5 V pixels.

**GRB order.** WS2812B pixels take their color data as green, then red, then blue. A program written for RGB shows red and green swapped until it uses GRB.

**Breadboard rail.** A long strip of connected holes along the edge of a breadboard, used for power or ground. Holes in the same column of five are connected; the two halves of the board are separate.
