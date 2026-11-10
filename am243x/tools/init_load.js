// Load one image on the LP-AM243x R5F core 0 over JTAG, after TI's device-manager init (the SDK's tools/ccs_load/am243x/load_dmsc_hsfs.js
// sequence: park the core, load sciclient_ccs_init, run it to its end). RAM only: nothing is written to flash.
// Run: /Applications/ti/ccs2101/ccs/scripting/run.sh am243x/tools/init_load.js <image.out>
const { join } = require("path");
const sdk = process.env.MCU_PLUS_SDK_PATH || join(process.env.HOME, "ti/mcu_plus_sdk_am243x_12_01_00_33");
const ds = initScripting();
ds.setScriptingTimeout(120000);
const image = process.argv[process.argv.length - 1];
const { cores } = ds.configure(join(__dirname, "..", "AM243x_LP_" + (process.env.AM243X_SERIAL || "S24L0096") + ".ccxml"));
const s = ds.openSession(cores.find(c => /Cortex_R5_0_0/.test(c)));
s.target.connect();
s.memory.fill(0x78000000, 0x800, 0, 32);
s.memory.write(0x78000000, [0xE59FF004, 0x38], 32);
s.memory.write(0x78000038, [0xEAFFFFFE], 32);
s.target.halt();
s.target.reset();
console.log("FRL_INIT start");
s.memory.loadProgram(join(sdk, "tools/ccs_load/am243x/sciclient_ccs_init.release.out"));
s.target.halt();
s.target.run(true);
console.log("FRL_INIT done");
s.target.reset();
s.memory.loadProgram(image);
s.target.run(false);
console.log("FRL_LOADED " + image);
ds.shutdown();
