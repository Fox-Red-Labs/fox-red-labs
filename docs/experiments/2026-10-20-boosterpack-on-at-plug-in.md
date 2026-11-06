<!-- experiment
id: E233
board: mspm33c321a, mspm0l1306
date: 2026-10-20
status: waiting
reached: configure
line: Not measured. Two LaunchPad kits with a BoosterPack stacked failed after a replug (one vanished from USB, one refused the debugger); both worked once the BoosterPacks were off.
stop: not run
-->
# Does a stacked BoosterPack stop the LP-MSPM33C321A or LP-MSPM0L1306 coming up after a replug?

Terms: [docs/vocab.md](../vocab.md). Related: [LP-MSPM33C321A FAQ](../../mspm33c321a/FAQ.md), [LP-MSPM0L1306 FAQ](../../mspm0l1306/FAQ.md).

**Question.**
Can a BoosterPack stacked on a LaunchPad keep the board off USB, or keep the debugger from connecting, after the USB cable is replugged?

**Vendor position.**
Not looked up. To read: the LaunchPad user's guides (power path from USB through the XDS110 to the target, 3V3 and 5V current limits, the isolation jumpers) and the BoosterPack's guide (current at power-up, pins it drives on the 40-pin header, anything on the SWD or reset pins).

**What was tried.**
Observed, not run as a test (2026-10-19 night and 2026-10-20 morning, during port runs):
- LP-MSPM33C321A (probe <probe serial>), Education BoosterPack MKII stacked: on USB, but the debugger refused to connect, also after an XDS110 reset.
- LP-MSPM0L1306 (probe <probe serial>), BOOSTXL-SHARP128 stacked: after a replug, the probe was gone from USB entirely.
- Both boards replugged with the BoosterPacks removed (2026-10-20 about 09:15): both on USB, and the MSPM33 debugger read flash at 0x10000000.

Two things changed at once (BoosterPack off, and a fresh replug), so this does not say which one fixed it.

**How far it got.**
Nothing measured.

### Expectation (written before the test)
- If the BoosterPack is the cause, a cold plug-in with it stacked fails most times, and the failure follows the BoosterPack to another LaunchPad.
- Likely mechanisms, in order: inrush or steady current on the shared 3V3 or 5V rail during enumeration on a bus-powered hub port (500 mA budget); a BoosterPack part driving nRESET, a boot pin or an SWD-shared pin at power-up; the MSPM33's own image putting the core in a state the debugger cannot reach (not related to the BoosterPack).
- If it is a replug race, failures happen at the same rate with the BoosterPack off.

### Plan
Ten cold plug-ins per condition, per board: BoosterPack on and off, through the hub and straight into the Mac. For each: does the probe enumerate (ioreg), does a flash read succeed, how long until it does. If a USB power meter is on hand, the current at plug-in. Then move the BoosterPack to a second LaunchPad. Runs after the port runs release the boards.

**The line.**
Not found. The BoosterPack on the board is the lead; a replug race is not ruled out.

**Ask.**
Has anyone seen a LaunchPad drop off USB, or refuse the debugger, only with a BoosterPack stacked?
