// Read the CPSW port 1 and port 2 frame counters through the debug port's system view, core left running.
// /Applications/ti/ccs2101/ccs/scripting/run.sh am243x/tools/cpsw_stats.js   (STAT_0 = MAC port 1 at 0x0803A000, STAT_1 = MAC port 2 at 0x0803A200)
const { join } = require("path");
const ds = initScripting(); ds.setScriptingTimeout(30000);
const { nonDebugCores } = ds.configure(join(__dirname, "..", "AM243x_LP_" + (process.env.AM243X_SERIAL || "S24L0096") + ".ccxml"));
const d = ds.openSession(nonDebugCores.find(c => /CS_DAP_0/.test(c)));
d.target.connect();
const rd = a => Number(d.memory.readOne("0x" + a.toString(16) + "@System_View", 32)) >>> 0;
const regs = [[0, "rx_good"], [4, "rx_broadcast"], [8, "rx_multicast"], [0x10, "rx_crc_err"], [0x30, "rx_octets"], [0x34, "tx_good"], [0x38, "tx_broadcast"], [0x3C, "tx_multicast"], [0x64, "tx_octets"]];
for (const [p, base] of [[1, 0x803A000], [2, 0x803A200]]) {
  const o = regs.map(([off, n]) => n + "=" + rd(base + off));
  console.log("FRL_CPSW port" + p + " " + o.join(" ") + " port_en=0x" + rd(0x8020014).toString(16));
}
ds.shutdown();
