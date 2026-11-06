<!-- experiment
id: E11
board: LP-MSPM0L1306
date: 2026-10-06
status: in progress
reached: link
line: Two functions in the IDE's code switch trace off on macOS, with the comment "not supported". TI's Mac trace server library cannot load as shipped, because of a path from TI's build machine and a Linux-style search path. Patched copies of the libraries load. No capture has run against a board.
stop: tools
-->
# Why does EnergyTrace on the TI LP-MSPM0L1306 not work on a Mac in CCStudio 21, and can it be made to?

**Short answer.** A switch in the IDE, plus two packaging mistakes in the trace server library and a search-path gap in the capture library, not a missing part. In CCStudio 21.0.1.00010 on macOS there is no Trace item under the Run menu, because two functions in the IDE's own code return "not supported" on macOS. Behind that switch, TI's Mac trace server library fails to load with `Library not loaded: @rpath/libti_logger.dylib` because of two packaging mistakes, and the capture library fails on its own with `Library not loaded: @rpath/libxdsfast3.dylib` (cause unknown, suspected packaging; the debugger may load it differently). With the switch removed and the library paths corrected in copies, the three libraries tested (`libCloudTrace.dylib`, `libenergytraceccs.dylib`, `libTraceFactory.dylib`) load. A capture has not been run.

Contents: the question, TI's position, what was found (the switch, then the libraries), the fixes, how far it got, the line, ideas, ask. Terms: [vocab](../vocab.md) (debug probe, EnergyTrace, dylib and rpath). Errors and fixes: [LP-MSPM0L1306 FAQ](../../mspm0l1306/FAQ.md).

**Question.** EnergyTrace is TI's current meter built into the XDS110-ET (the onboard XDS110 debug probe of the LaunchPad development kit, with the EnergyTrace circuit), on many LaunchPad kits including the LP-MSPM0L1306 (SLAU869E) and LP-MSPM0G3507 (its user's guide SLAU873), and in the LP-XDS110ET clip-on probe (SPRUJ01). It worked on macOS before: TI staff ran it on macOS 10.15 with CCStudio 9.3 and 10 (E2E threads 880175 and 918854, 2020; E2E is TI's public support forum), and a user reports it working in CCStudio 12.8.1 (thread 1466304). In CCStudio 21.0.1 on this Mac, Run has no Trace item (seen 2026-10-06, no screenshot saved). Is that a missing driver, a missing tool, or a switch?

**Vendor position.** The sources in this section were read on 2026-10-06 and are quoted here; none are saved. CCStudio 21.0.1 user's guide, section "Trace": "EnergyTrace™ is available on CCStudio IDE Windows and Linux desktop hosts only." The 20.3, 20.4 and Theia 1.4 and 1.5 guides say the same with the older product name, and say it for Core Trace too. No reason is given. On E2E, a user on CCStudio 20.0.0 for macOS with an LP-MSPM0G3507 reported no Trace item and a gray EnergyTrace button in the Trace view, and was told by TI staff to go back to CCStudio 12.8: "there still a little feature gap between CCS 12.8 and CCS 20. These feature will release in the future" (thread 1466304, 2025). TI staff also say the last macOS that CCStudio 12.8 supports is Ventura (thread 1579854, 2026). TI's bug tracker entry EXT_EP-12168 (opened 2025-01-21, still open when read on 2026-10-06) covers standalone EnergyTrace in the new IDE, with the workaround "Use CCS 12.x". The XDS110 user's guide SPRUI94, section 3.6.5, documents the command-line capture tool `stune` for Windows only, and the Mac install has no file by that name. The LP-MSPM0L1306 user's guide SLAU869E, section 2.3.2, says the onboard XDS110-ET supports EnergyTrace.

**What was tried.** Intel Mac, macOS 15.7.9, CCStudio 21.0.1.00010 with DSLite 21.0.0.4166 (DSLite is the debugger process the IDE talks to), checked 2026-10-06. No board was needed. The IDE is built on Theia (an open-source editor framework, like VS Code) and ships its own TypeScript source inside `Code Composer Studio.app/Contents/Resources/app.asar` (an asar is Electron's archive format, one file holding the app's files), so it can be read. Three of the 75 libraries under `ccs_base/emulation/analysis/bin` were then checked with `otool` (Apple's tool that lists what a library links to and where it searches) and a load test from Python (`ctypes.CDLL`, which calls `dlopen`, the same loader the IDE uses). The other 72 were not tested. The claims below come from `otool`, `dlopen`, `codesign`, `file`, `nm`, `strings` and `grep` output; anything not checked that way is marked.

The switch, in the IDE:

- `@ccs/ccstudio-trace-config`, file `trace-menu-contribution.ts`, function `isTraceSupported()`: returns false when its helper `isMacOS()` is true, and that helper returns `CCSIDE.onMacOS`. The code comment reads "trace is not supported on Mac OS, only windows and linux".
- `@ccs/ccstudio-trace-viewer`, file `trace-feature-commands.ts`, function `doesPlatformSupportTrace()`: tests `CCSIDE.onMacOS` directly, same comment.

The first runs in the trace menu class's constructor, once at start-up (line 133 of that file), and the Trace menu and its commands register only when it is true. Core Trace (the feature behind the same menu that records which instructions ran) sits behind the same hidden menu, which matches the user's guide listing both as Windows and Linux only. The comment says "not"; TI gives no reason in the code or the docs. The comment appears 8 times in the archive: the two TypeScript sources, the two compiled package copies, the two page bundles the app runs, and two source maps.

What the Mac install already contains for trace: `libenergytraceccs.dylib` (the EnergyTrace capture code, with decoders for MSP432-class and wireless probes and calibration), `libmsp430.dylib` with `MSP430_EnableEnergyTrace`, the XDS110 JTAG drivers with `energytrace_start` and `energytrace_stop`, `libCloudTrace.dylib` (TI's own trace server: a websocket server, a long-lived local network connection, on port 1239 by default, that the IDE's trace views read; it does not need Eclipse Trace Compass, the open-source trace viewer that is an off-by-default preference here; Java was not checked), and the trace data file tools `td` and `bin2tdf`. Not checked: whether the XDS110-ET probe firmware path works with a Mac host, and the other 72 libraries in the folder.

The libraries, behind the switch:

- `libCloudTrace.dylib` depends on `<ti-build-path> an absolute path on TI's build server. A `libsqlite3.dylib` sits next to it, but its own install name is that same build-server path, so it is never matched.
- Its only search path (rpath) is `$ORIGIN/../../../DebugServer/bin:$ORIGIN/../../../common/bin:$ORIGIN/`. `$ORIGIN` and the colon list are Linux conventions; macOS uses `@loader_path` and one path per entry, so it is expected to treat this as a single folder that does not exist (the load error that follows fits that; the loader was not traced). A Linux search path shipped on macOS. Loading the library as shipped fails:

```text
OSError: dlopen(.../libCloudTrace.dylib, 0x0006): Library not loaded: @rpath/libti_logger.dylib
```

- `libenergytraceccs.dylib` searches only its own folder (`@loader_path/`), but the probe driver it needs (`libxdsfast3.dylib`, `libjscxds110.dylib`) lives in `ccs_base/common/uscif`. As shipped it fails:

```text
OSError: dlopen(.../libenergytraceccs.dylib, 0x0006): Library not loaded: @rpath/libxdsfast3.dylib
```

  **Unknown (suspected: Tool or docs).** It may still load inside the debugger, which loads the probe driver first; that was not tested.

- Same pattern, not trace: `libxerces-c.dylib` and `libCCReg.dylib` in `common/bin` carry `$ORIGIN` search paths, and the two disassembler libraries in `emulation/tpi` point at `<ti-build-path> `.so` files.

The fixes, made on copies. The originals, the patched files and their checksums are kept outside this repo because they are TI's binaries; the kit is not published. Quit CCStudio before touching the install, and keep the originals to put back.

1. The switch. In a copy of `app.asar`, the expression `general_1.CCSIDE.onMacOS` occurs 16 times; the four inside `isMacOS()` and `doesPlatformSupportTrace()` (two in the page bundles the app runs, two in the compiled package copies) were replaced by `false` padded with spaces to the same 24 bytes, which keeps the archive's byte layout; whether the app launches with the patched copy is not tested. The other 12 are path helpers and were left alone.
2. The trace server library, with `install_name_tool` (Apple's tool for editing a library's dependency names and search paths):

```text
install_name_tool -id @rpath/libsqlite3.dylib libsqlite3.dylib
```

```text
install_name_tool -change <ti-build-path> @rpath/libsqlite3.dylib libCloudTrace.dylib
```

```text
install_name_tool -delete_rpath '$ORIGIN/../../../DebugServer/bin:$ORIGIN/../../../common/bin:$ORIGIN/' libCloudTrace.dylib
```

```text
install_name_tool -add_rpath @loader_path/../../../DebugServer/bin -add_rpath @loader_path/../../../common/bin -add_rpath @loader_path/ libCloudTrace.dylib
```

3. The capture library:

```text
install_name_tool -add_rpath @loader_path/../../../common/uscif -add_rpath @loader_path/../../../common/bin -add_rpath @loader_path/../../../DebugServer/bin libenergytraceccs.dylib
```

The five files checked (`libCloudTrace.dylib`, `libenergytraceccs.dylib`, `libsqlite3.dylib`, DSLite and the app) are not code-signed (`codesign -dvv` reports "code object is not signed at all"); the other libraries they load were not checked. Unsigned files are expected not to trip macOS library validation.

**How far it got.** The steps below mean library loading, not firmware; nothing was compiled.

- Configure: the switch found and removed in a copy of the IDE bundle.
- Link: as shipped, `libCloudTrace.dylib` and `libenergytraceccs.dylib` fail with the errors above; `libTraceFactory.dylib` loads. The patched copies were placed in a mirror of the install tree (the new search paths are relative to the library's own folder) and all three load.
- Run: not. Needs CCStudio quit, the four patched files copied in, a restart, a debug connection on the LP-MSPM0L1306, then Run, Trace, Enable EnergyTrace.

**The line.** As shipped, the Mac build stops before the first byte of trace: the menu is hidden by two functions, and the trace server library cannot be loaded. On a Mac, what the debugger does once EnergyTrace is enabled was not known at the time; the working guess was that it would capture, because the capture code is present and loads, and the switch's own comment says "not" rather than "cannot". Not checked: the probe firmware path, a real capture, Apple silicon (the three libraries tested and DSLite are x86_64, Intel, builds; TI staff say the debug stack runs there under Rosetta, Apple's Intel-to-Arm translator, thread 1579854, 2026).

- The run itself, then a check against a meter across the LP-MSPM0L1306's J101 3V3 jumper (the user's guide's own current-measurement point, SLAU869E section 2.6). Until then, the ways to measure current on a Mac today are that meter or a Nordic Power Profiler Kit II, whose app runs on macOS.
- A command-line capture for Mac and Linux. The trace server is a websocket on a known port and the IDE's source shows the three calls it makes (lines 979, 906 and 1019 of `trace-menu-contribution.ts`): `getLiveTraceReceivers`, `configureTargetSession` with `EnergyTrace.enable`, then a `DataReady` event. A small script against a headless DSLite could give Mac and Linux the `stune` that Windows has.
- Linux instead of Mac. TI ported its `stune` command-line capture to Linux (tracker EXT_EP-9143, declined for macOS: "This was ported to Linux. There is no plan to support MacOS at this time"). A Linux machine or a Linux virtual machine with USB passthrough gets a scripted EnergyTrace with no IDE. Nobody has published a tool that talks to the XDS110's EnergyTrace endpoint directly; the open `energytrace-util` covers MSP430 probes only (checked 2026-10-07).

**Ask.** Anyone who has captured EnergyTrace on a Mac, from the IDE or a script: which version, and which steps?

Update 2026-10-07: the patched CCStudio starts and debugs, but its Mac debugger (DSLite) answers `Unknown sub module: "trace"` when EnergyTrace is enabled, so the IDE path stops there. The way round it is `tools/energytrace/etcap.py`, which drives the probe through CCStudio's own driver library with no IDE; first numbers are rows in `data/results/mspm0l1306.csv`, each with the board's 1.500 mA SHUTDOWN reading subtracted. Open: the J101 jumper test on the L1306 to learn whether that 1.5 mA is a floor of the probe.
