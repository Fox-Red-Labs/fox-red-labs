// Issue the debugger's System Reset on the LP-AM243x (connect without a GEL, reset, disconnect). Peripherals, MSS_CTRL
// and the clusters return to their reset values (both clusters come back in lockstep; the next connect with TI's usual
// connection file switches them to dual-core). Run: /Applications/ti/ccs2101/ccs/scripting/run.sh am243x/tools/sysreset.js
const { join } = require("path");
const ds = initScripting();
ds.setScriptingTimeout(60000);
const { cores } = ds.configure(join(__dirname, "..", "AM243x_LP_" + (process.env.AM243X_SERIAL || "S24L0096") + ".ccxml"));
const s = ds.openSession(cores.find(c => /Cortex_R5_0/.test(c)));
s.target.connect();
s.target.reset("System Reset");
console.log("FRL_SYSRESET done");
ds.shutdown();
