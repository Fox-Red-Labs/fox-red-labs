// Read-only: where is a CC23xx or CC27xx chip in its boot, and is its debug port (AHB-AP) open?
// Connects to the DAP core only (no CPU connect, no reset, no write) and reads CFG-AP DEVICESTATUS.
// Run with the CCStudio scripting runner and the board's .ccxml (pick the probe by serial number in the .ccxml):
//   /Applications/ti/ccs2101/ccs/scripting/run.sh tools/recovery/cc2x_status.js cc2755p10/CC2755P10.ccxml
// Boot status names are from the CC27xx TRM (SWCU195A, Table 9-2); the CC23xx TRM uses the same scheme.
const ccxml = process.argv[process.argv.findIndex(x => x.endsWith("cc2x_status.js")) + 1];
const NAMES = {
  0x00: "BOOT_RESET", 0x01: "BOOT_COLD_BOOT", 0x02: "BOOT_SRAM_REP_DONE", 0x03: "BOOT_GENERAL_TRIMS",
  0x20: "BOOT_ENTERED_SACI (waiting for device management commands)", 0x36: "BOOT_WAIT_SWD_DISCONNECT",
  0x37: "BOOT_EXITED_SACI", 0x38: "BOOT_WAITLOOP_DBGPROBE (flashless test mode)", 0x3E: "BOOT_FAIL_SRAM_REPAIR",
  0x3F: "BOOT_FAULT_HANDLER (stuck until reset)", 0x80: "BLDR_MODE_ENTRY", 0x81: "BLDR_WAITLOOP_DBGPROBE",
  0xBA: "ROM serial bootloader started (normal for a chip with no valid configuration)", 0xBB: "BLDR_CMD_IDLE",
  0xBC: "BLDR_CMD_PROCESSING", 0xBF: "BLDR_FAULT_HANDLER", 0xC0: "APP_MODE_ENTRY", 0xC1: "APP_WAITLOOP_DBGPROBE",
  0xC2: "BLDR_START_INIT (boot done, application starting)", 0xFD: "APP_FAIL_NOAPP", 0xFE: "APP_FAIL_APPTRANSFER",
  0xFF: "APP_FAULT_HANDLER (bad vector table in CCFG.bootCfg.pAppVtor)",
};
const ds = initScripting(); ds.setScriptingTimeout(30000);
ds.configure(ccxml);
const s = ds.openSession(/CS_DAP/);
s.target.connect();
const v = Number(BigInt.asUintN(32, BigInt(s.expressions.evaluate("'REG'::CFGAP_DEVICESTATUS"))));
const boot = (v >>> 8) & 0xff;
console.log("DEVICESTATUS 0x" + v.toString(16).padStart(8, "0"));
console.log("boot status 0x" + boot.toString(16) + " " + (NAMES[boot] || (boot >= 0xC3 ? "set by the application" : "reserved")));
console.log("debug port (AHB-AP) " + (((v >>> 24) & 1) ? "open" : "closed"));
s.target.disconnect(); ds.shutdown();
