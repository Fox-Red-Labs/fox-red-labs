// Load a RAM image on the F2800137 (CL371081 ccxml), run it free, leave it running. run.sh run.js <prog.out>
const { join } = require("path");
const prog = process.argv[process.argv.findIndex(x => x.endsWith(".out"))];
const ds = initScripting();
ds.setScriptingTimeout(120000);
ds.configure(join(__dirname, "F2800137_CL371081.ccxml"));
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
s.target.reset();
s.memory.loadProgram(prog);
s.target.run(false);
console.log("running");
const t = Date.now(); while (Date.now() - t < 14000) {}
console.log("halted? " + s.target.isHalted());
s.target.disconnect();
