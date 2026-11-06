// Is the R5F's TCM 64 kB per bank in lockstep and 32 kB in split mode (TI's smart-placement page)? On core 0, write a
// different word at offset 0x4000 and 0xC000 of TCM-A (0x0) and TCM-B (0x80000), read both back. If the upper half is
// not separate memory, the 0xC000 write either fails or lands on 0x4000. MODE=lockstep: System Reset and TI's GEL init
// without the dual-core switch (as lockstep.js). MODE=split: connect with TI's usual connection file.
const { join } = require("path");
const GELDIR = "/Applications/ti/ccs2101/ccs/ccs_base/emulation/gel/AM263Px";
const SER = process.env.AM263P_SERIAL || "S26E0426", MODE = process.env.MODE || "lockstep";
const ds = initScripting();
ds.setScriptingTimeout(120000);
const { cores } = ds.configure(join(__dirname, "..", (MODE === "split" ? "AM263Px_ZCZ_C_" : "AM263Px_nogel_") + SER + ".ccxml"));
const s = ds.openSession(cores.find(c => /Cortex_R5_0$/.test(c)));
s.target.connect();
try { s.target.halt(); } catch (e) {}
if (MODE === "lockstep") {
  s.target.reset("System Reset");
  try { s.target.halt(); } catch (e) {}
  s.expressions.evaluate(`GEL_LoadGel("${GELDIR}/AM263Px.gel")`);
  for (const f of ["MSS_CTRL_unlock", "MSS_TOPRCM_unlock", "MSS_RCM_unlock", "MSS_IOMUX_Unlock", "TOP_CTRL_unlock", "R5F_ROM_eclipse",
    "R5F_SS0_CoreA_unhalt", "R5F_SS1_CoreA_unhalt", "MSS_L2_Mem_Init", "MailBox_MEM_Init",
    "Configure_Plls_R5F_400_SYS_200_Clocks", "Configure_All_Peripheral_Clks"]) s.expressions.evaluate(f + "()");
}
const ev = e => { try { return s.expressions.evaluate(e); } catch (x) { return "ERR " + String(x.message || x).split("\n")[0].slice(0, 120); } };
const hx = v => typeof v === "bigint" ? "0x" + v.toString(16) : String(v);
console.log("FRL_TCM mode", MODE, "SS0_STATUS", hx(ev("*(unsigned int *)0x50D0002C")));
for (const [name, base] of [["TCMA", 0x0], ["TCMB", 0x80000]]) {
  const lo = base + 0x4000, hi = base + 0xC000;
  ev(`*(unsigned int *)0x${lo.toString(16)} = 0x11111111`);
  const w = ev(`*(unsigned int *)0x${hi.toString(16)} = 0x22222222`);
  console.log("FRL_TCM", name, `write 0x${hi.toString(16)}:`, typeof w === "string" ? w : "ok",
    `read 0x${lo.toString(16)}=${hx(ev(`*(unsigned int *)0x${lo.toString(16)}`))}`,
    `0x${hi.toString(16)}=${hx(ev(`*(unsigned int *)0x${hi.toString(16)}`))}`);
}
ds.shutdown();
