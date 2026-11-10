// LP-AM243 revision E2 only: drive GPIO0_70 (PRG_CPSW_RGMII1_MUX_SEL on E2, SPRUJ12F appendix A table A-2) high to select CPSW, the core left running.
// On revision E3 that pin is an input from the second PHY's LED line; E3 uses GPIO0_27, which TI's enet_lwip_cpsw already drives.
// /Applications/ti/ccs2101/ccs/scripting/run.sh am243x/tools/e2_mux.js
const ds = initScripting(); ds.setScriptingTimeout(30000);
const { nonDebugCores } = ds.configure(require("path").join(__dirname, "..", "AM243x_LP_" + (process.env.AM243X_SERIAL || "S24L0096") + ".ccxml"));
const d = ds.openSession(nonDebugCores.find(c => /CS_DAP_0/.test(c))); d.target.connect();
const rd = a => Number(d.memory.readOne("0x" + a.toString(16) + "@System_View", 32)) >>> 0;
const wr = (a, v) => d.memory.write("0x" + a.toString(16) + "@System_View", [v >>> 0], 32);
wr(0xF1008, 0x68EF3490); wr(0xF100C, 0xD172BC5A);          // pad config unlock
wr(0xF411C, 7 | (1 << 18) | (1 << 17));                      // PRG1_PRU1_GPO5 (AA11) as GPIO0_70, pull up
const g = 0x600000;
wr(g + 0x60, rd(g + 0x60) & ~(1 << 6));                      // direction: output
wr(g + 0x68, 1 << 6);                                        // set GPIO0_70 high
console.log("FRL_E2MUX pad=" + rd(0xF411C).toString(16) + " dir45=" + rd(g + 0x60).toString(16) + " out45=" + rd(g + 0x64).toString(16));
ds.shutdown();
