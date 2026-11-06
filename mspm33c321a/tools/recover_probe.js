// Step 1 (no flash writes): read boot diagnostic over the DAP, then try a wait-for-debug connect to the M33 and halt.
const ccxml = process.argv[process.argv.findIndex(x => x.endsWith("probe.js")) + 1];
const hex = v => "0x" + (Number(BigInt.asUintN(32, BigInt(v)))).toString(16).padStart(8, "0");
const ds = initScripting(); ds.setScriptingTimeout(60000);
ds.configure(ccxml);
const d = ds.openSession(/CS_DAP/); d.target.connect();
const rd = r => { try { return hex(d.expressions.evaluate("'REG'::" + r)); } catch (e) { return "ERR " + e.message.split("\n")[0]; } };
for (const r of ["CFGAP_BOOTDIAG", "CFGAP_DEVICESTATUS", "PWRAP_DPREC0", "PWRAP_SPREC"]) console.log(r, rd(r));
const m = ds.openSession(/CORTEX_M33/);
try { m.settings.set("EnableWaitForDebug", true); console.log("EnableWaitForDebug set"); } catch (e) { console.log("set failed", e.message); }
try {
  m.target.connect(); console.log("M33 connected");
  if (!m.target.isHalted()) m.target.halt();
  console.log("halted", m.target.isHalted(), "PC", hex(m.registers.read("PC")));
  console.log("0x10000000:", m.memory.read(0x10000000n, 8).map(hex).join(" "));
  m.target.disconnect();
} catch (e) { console.log("M33 connect failed:", e.message.split("\n").slice(0,4).join(" | ")); }
for (const r of ["CFGAP_BOOTDIAG", "CFGAP_DEVICESTATUS"]) console.log(r, rd(r));
d.target.disconnect(); ds.shutdown();
