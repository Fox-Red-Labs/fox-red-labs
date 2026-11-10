// Read (and optionally change) the Ethernet PHYs' registers through the CPSW MDIO block, and dump the CPSW port counters, core left running.
// /Applications/ti/ccs2101/ccs/scripting/run.sh am243x/tools/phy_probe.js            reads PHY 3 and 15 and the nonzero counters of MAC port 1
// PHY_FORCE100=<addr> removes 1000BASE-T from that PHY's advertisement (register 9 bits 8 and 9) and restarts autonegotiation.
const { join } = require("path");
const ds = initScripting(); ds.setScriptingTimeout(30000);
const { nonDebugCores } = ds.configure(join(__dirname, "..", "AM243x_LP_" + (process.env.AM243X_SERIAL || "S24L0096") + ".ccxml"));
const d = ds.openSession(nonDebugCores.find(c => /CS_DAP_0/.test(c)));
d.target.connect();
const rd = a => Number(d.memory.readOne("0x" + a.toString(16) + "@System_View", 32)) >>> 0;
const wr = (a, v) => d.memory.write("0x" + a.toString(16) + "@System_View", [v >>> 0], 32);
const UA = 0x8000F80;
const wait = () => { for (let i = 0; i < 1000; i++) { const v = rd(UA); if (!(v & 0x80000000)) return v; } return null; };
const mdioRead = (p, r) => { if (wait() === null) return null; wr(UA, (0x80000000 | (r << 21) | (p << 16)) >>> 0); const v = wait(); return v !== null && (v & 0x20000000) ? v & 0xFFFF : null; };
const mdioWrite = (p, r, x) => { wait(); wr(UA, (0x80000000 | 0x40000000 | (r << 21) | (p << 16) | x) >>> 0); wait(); };
const h = v => v === null ? "none" : "0x" + v.toString(16);
const regs = [[2, "id1"], [3, "id2"], [0, "bmcr"], [1, "bmsr"], [4, "anar"], [5, "anlpar"], [9, "gbcr"], [10, "gbsr"], [0x15, "reg15_rxerr"], [0x10, "reg10"], [0x11, "reg11"]];
const force = process.env.PHY_FORCE100;
if (force) { const p = Number(force); const g = mdioRead(p, 9); mdioWrite(p, 9, g & ~0x0300); const b = mdioRead(p, 0); mdioWrite(p, 0, (b | 0x1200) & ~0x0800 & 0xFFFF); console.log("FRL_FORCE100 phy" + p + " gbcr " + h(g) + " -> " + h(mdioRead(p, 9))); }
for (const p of [3, 15]) console.log("FRL_PHY" + p + " " + regs.map(([r, n]) => n + "=" + h(mdioRead(p, r))).join(" "));
const o = []; for (let off = 0; off < 0x100; off += 4) { const v = rd(0x803A000 + off); if (v) o.push("0x" + off.toString(16) + "=" + v); }
console.log("FRL_CPSW_P1_nonzero " + o.join(" "));
const o2 = []; for (let off = 0; off < 0x100; off += 4) { const v = rd(0x803A200 + off); if (v) o2.push("0x" + off.toString(16) + "=" + v); }
console.log("FRL_CPSW_STAT1_nonzero " + o2.join(" "));
ds.shutdown();
