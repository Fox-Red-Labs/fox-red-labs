# Four ways to blink an MSPM0 LaunchPad: CCStudio, VS Code, terminal or Zephyr

You do not need Code Composer Studio (CCStudio) to blink an MSPM0 LaunchPad, but one path needs it for an AI assistant to drive the board. Pick the path that matches the tools you already use. Facts checked against public TI docs, the VS Code marketplace and the Zephyr project on 2026-10-04. Terms: [vocab](vocab.md).

## The one fact that decides most of it

TI's four MCP servers (project build, debug, SysConfig, serial) are started by CCStudio and only run while CCStudio is open with an AI-enabled workspace. There is no standalone download of them. So:

- Want the agent to build, flash, set breakpoints and read variables through TI's own servers? CCStudio has to be installed and running.
- Want to type in VS Code or a terminal instead of CCStudio's editor? Fine. TI documents the MCP JSON for any client, and Claude Code in a terminal attaches to the same servers while CCStudio sits in the background.
- Want no CCStudio at all? Then the agent gets a compiler and a flasher, and debugging comes from OpenOCD and GDB, which is a different, rougher experience.

## Path A: TI's front door (CCStudio + Claude Code inside it)

Install: CCStudio 21.0.1, MSPM0 SDK, turn on the AI Assistant Configurator.
What you get: the full agent loop. It edits the SysConfig file, rebuilds, flashes, runs, reads the UART, sets breakpoints.
Easy: one installer, one checkbox, the agent sees the hardware.
Hard: CCStudio is a 1 GB Theia IDE. First launch, workspace concepts and the auto-generated CLAUDE.md block take some getting used to.
Board: any MSPM0 LaunchPad, including the LP-MSPM0C1104.
Timed on the LP-MSPM0G3507: [mspm0g3507/README.md](../mspm0g3507/README.md).

## Path B: VS Code or terminal, CCStudio hidden in the background

Install: everything in Path A, plus Claude Code CLI and the MCP JSON from TI's AI chapter with Mac paths.
What you get: Path A's agent loop from the editor you already use. CCStudio stays open but you never look at it.
Easy: same capability as A; your own editor.
Hard: two apps running, one of them only as a daemon; the MCP servers die when CCStudio closes; path strings in the JSON are platform-specific.
Board: same as A.
Fits anyone who already works in VS Code.

## Path C: no CCStudio. Compiler, SysConfig CLI, make, OpenOCD

Install: TI Arm Clang (free standalone download), SysConfig standalone, MSPM0 SDK (the examples ship makefiles), TI Embedded Debug for VS Code (TI's own extension: OpenOCD build with XDS110 support, GDB, Cortex-Debug).
What you get: `make` builds the example, OpenOCD flashes it, VS Code debugs it. The agent gets a shell, not TI's MCP servers. Third-party OpenOCD MCP servers exist and are chip-agnostic.
Easy: no IDE; everything is a command; feels like ESP-IDF or Pico SDK.
Hard: four separate installs with paths to wire together; TI's own forum shows the VS Code debug extension failing for users where CCStudio worked; MSPM0 has a NONMAIN flash region that can brick the chip permanently if a flash operation is interrupted, and the safety rails for that live in CCStudio's flash loader.
Board: any MSPM0 LaunchPad.
This path exists and works, with the most setup of the four.

## Path D: Zephyr

Install: Zephyr SDK and west, a custom OpenOCD build (the Zephyr SDK's OpenOCD does not support the XDS110 probe; TI's own Zephyr guide says so).
What you get: `west build -b lp_mspm0g3507 samples/basic/blinky` and `west flash`. The Arduino and ESP32 crowd's mental model.
Easy: one build system across vendors; the agent already knows Zephyr well.
Hard: board support is upstream for LP-MSPM0G3507 (status Maintained) and in TI's downstream for G3519 and L2228, not for the C1104; the custom OpenOCD step; a documented case in Dec 2025 of two LaunchPad kits left unreachable after `west flash`. The cause was a clock driver bug that stopped the CPU, not a NONMAIN write; the fix is Zephyr PR 102668, merged 2026-02-03.
Board: LP-MSPM0G3519 (already in the TI cart) or LP-MSPM0G3507. Not the C1104.

## Not a path today

Arduino and PlatformIO: no official MSPM0 core. See [ports.md](ports.md) for the Arduino layer this repo ships.

## Which path to start with

1. New to TI tools: Path A on any MSPM0 LaunchPad, such as the LP-MSPM0C1104.
2. Already in VS Code: Path B, which keeps Path A's agent tools.
3. Want no IDE: Path C. Keep a spare board, because an interrupted flash can lock a chip.
4. Already use Zephyr: Path D on the LP-MSPM0G3507 or LP-MSPM0G3519, not the LP-MSPM0C1104.

## Sources

- CCStudio AI chapter, MCP servers only while CCStudio runs, JSON for other clients: <https://software-dl.ti.com/ccs/esd/documents/users_guide/ccs_ai.html>
- TI Embedded Debug for VS Code: <https://marketplace.visualstudio.com/items?itemName=ti-development-tools.ti-embedded-debug>
- TI E2E, VS Code debug extension failing where Theia works (MSPM0G3507): <https://e2e.ti.com/support/microcontrollers/arm-based-microcontrollers-group/arm-based-microcontrollers/f/arm-based-microcontrollers-forum/1549606/mspm0g3507-ti-embedded-debug-vscode-extension-not-working>
- Zephyr board page, lp_mspm0g3507: <https://docs.zephyrproject.org/latest/boards/ti/lp_mspm0g3507/doc/index.html>
- TI downstream Zephyr support (G3507, G3519, L2228; XDS110 not in stock OpenOCD): <https://github.com/TexasInstruments/msp-zephyr>
- Zephyr issue, LaunchPad bricked by west flash, fixed Jan 2026: <https://github.com/zephyrproject-rtos/zephyr/issues/100734>
- TI guide to Zephyr on MSPM0 (SLUUDI9): <http://ti.com/lit/pdf/sluudi9>
