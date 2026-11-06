// Run a program on R5FSS0-0 with the AM263P's R5F clusters in lockstep, from the debugger (development boot).
// The chip comes out of a system reset in lockstep (MSS_CTRL R5SS0_CONTROL reset value 7, R5SS0_STATUS_REG lock-step
// bit 0x100). TI's usual connection script switches both clusters to dual-core on every connect, and TI's lockstep
// script only sets FORCE_WFI, which does not bring LOCK_STEP back once a dual-core connect has cleared it. So: connect
// without a GEL, issue the debugger's System Reset, then call TI's own GEL steps for clocks and memory init, leaving out
// the dual-core switch. Env: IMAGE (absolute .out), RUN_MS (how long to keep the session open while the program runs).
// Prints FRL_LS lines: register values before and after, whether Cortex_R5_1 connects, load result.
// Run: /Applications/ti/ccs2101/ccs/scripting/run.sh am263p/tools/lockstep.js
const { join } = require("path");
const GELDIR = "/Applications/ti/ccs2101/ccs/ccs_base/emulation/gel/AM263Px";
const ds = initScripting();
ds.setScriptingTimeout(120000);
const { cores } = ds.configure(join(__dirname, "..", "AM263Px_nogel_" + (process.env.AM263P_SERIAL || "S26E0426") + ".ccxml"));
const s = ds.openSession(cores.find(c => /Cortex_R5_0$/.test(c)));
s.target.connect();
try { s.target.halt(); } catch (e) {}
const rd = a => { try { return "0x" + s.expressions.evaluate(`*(unsigned int *)0x${a.toString(16)}`).toString(16); } catch (e) { return "ERR"; } };
const regs = t => console.log("FRL_LS", t, ["SS0_CONTROL", 0x50D00020, "SS0_STATUS", 0x50D0002C, "SS0_FORCE_WFI", 0x50D00038,
  "SS1_CONTROL", 0x50D00040, "SS1_STATUS", 0x50D0004C, "SS1_FORCE_WFI", 0x50D00058].reduce((o, x, i, a) => i % 2 ? o : o + ` ${x}=${rd(a[i + 1])}`, ""));
regs("connected");
s.target.reset("System Reset");
try { s.target.halt(); } catch (e) {}
regs("after_system_reset");
s.expressions.evaluate(`GEL_LoadGel("${GELDIR}/AM263Px.gel")`);
for (const f of ["MSS_CTRL_unlock", "MSS_TOPRCM_unlock", "MSS_RCM_unlock", "MSS_IOMUX_Unlock", "TOP_CTRL_unlock", "R5F_ROM_eclipse",
  "R5F_SS0_CoreA_unhalt", "R5F_SS1_CoreA_unhalt", "MSS_L2_Mem_Init", "MailBox_MEM_Init",
  "Configure_Plls_R5F_400_SYS_200_Clocks", "Configure_All_Peripheral_Clks"]) s.expressions.evaluate(f + "()");
regs("after_init");
for (const n of [1, 3]) {   // the checker core of each cluster: core 1 (R5FSS0-1) and core 3 (R5FSS1-1)
  try {
    const sx = ds.openSession(cores.find(c => new RegExp("Cortex_R5_" + n + "$").test(c)));
    sx.target.connect(); console.log(`FRL_LS core${n}_connect ok`); sx.target.disconnect();
  } catch (e) { console.log(`FRL_LS core${n}_connect failed:`, String(e.message || e).split("\n")[0].slice(0, 200)); }
}
const img = process.env.IMAGE;
if (img) {
  s.memory.loadProgram(img);
  console.log("FRL_LS loaded", img.split("/").pop());
  s.target.run(false);
  const ms = Number(process.env.RUN_MS || 20000);
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  try { s.target.halt(); } catch (e) {}
  regs("after_run");
}
ds.shutdown();
