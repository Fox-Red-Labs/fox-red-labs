// Probe actions for the LAUNCHXL-F2800137 port tests, through CCStudio scripting. Probe CL371081 only (the connection file names it).
//   run.sh f28probe.js flash <prog.out>   load into flash with the debugger's flash tool, System Reset, run
//   run.sh f28probe.js reset              System Reset, run
//   run.sh f28probe.js pins               sample GPADAT 24 times, 70 ms apart
//   run.sh f28probe.js readlog <out.bin>  read the 2 kB log sector (sector 100, 0x99000)
const { join } = require("path");
const a = process.argv.slice(process.argv.findIndex(x => x.endsWith("f28probe.js")) + 1);
const ds = initScripting();
ds.setScriptingTimeout(180000);
ds.configure(join(process.env.FRL_ROOT, "c2000-launchpad/tools/F2800137_CL371081.ccxml"));
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
if (a[0] === "flash") {
  s.target.reset("System Reset");
  s.memory.loadProgram(a[1]);
  console.log("loaded " + a[1]);
  s.target.reset("System Reset");
  s.target.run(false);
} else if (a[0] === "reset") {
  s.target.reset("System Reset");
  s.target.run(false);
} else if (a[0] === "pins") {              // CPU reset, PC to the flash entry, run, wait 6 s (past the boot messages), then sample GPADAT 24 times 70 ms apart (GPIO20 is LED4)
  s.target.reset();                       // a System Reset leaves the CPU halted in the boot ROM, where run(false) does not resume it
  s.registers.write("PC", 0x80000);       // flash entry (codestart branch)
  s.target.run(false);
  const t6 = Date.now(); while (Date.now() - t6 < 6000) {}
  const sleep = (ms) => { const t = Date.now(); while (Date.now() - t < ms) {} };
  const out = [];
  for (let i = 0; i < 24; i++) { sleep(70); out.push(Number(s.memory.readOne(0x7F00, 32))); }
  console.log(out.map(v => `GPIO20=${(v >> 20) & 1}`).join(" "));
} else if (a[0] === "readlog") {
  s.memory.saveBinary(0x99000, 0x400, a[1]);
}
s.target.disconnect();
ds.shutdown();
