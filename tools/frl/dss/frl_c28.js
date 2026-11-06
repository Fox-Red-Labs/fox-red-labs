// frl's C2000 operations through CCStudio scripting (OpenOCD has no C28x support). One debug session per call.
//   run.sh frl_c28.js <ccxml> identity
//   run.sh frl_c28.js <ccxml> backup  <addr> <words> <out.bin>          read-only
//   run.sh frl_c28.js <ccxml> program <image.out>                        CCStudio's flash tool: main flash only
//   run.sh frl_c28.js <ccxml> restore <in.bin> <addr> <words> <back.bin> write back, verify, read back
//   run.sh frl_c28.js <ccxml> sample  <dat> <dir> <bit> <seconds>        LED pin while the program runs
//   run.sh frl_c28.js <ccxml> verify  <image.out>                        flash against the image, read-only
// Addresses are C28x word addresses. Output lines are what tools/frl/dss.py parses ("ID", "T", "N", "E", "OK").
// Generalized from c2000-launchpad/tools/c2kbackup.js, c2kflashback.js and c2krun.js.
const a = process.argv.slice(process.argv.findIndex(x => x.endsWith("frl_c28.js")) + 1);
const [ccxml, op] = a;
const num = (s) => parseInt(s, s.startsWith("0x") ? 16 : 10);
const ds = initScripting();
ds.setScriptingTimeout(600000);
ds.configure(ccxml);
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
// Start the program in flash the way a flash boot would. With a debugger attached the C2000 takes the emulation
// boot path and waits in the boot ROM, so a chip found there is sent to the flash entry point (route target.entry).
const ENTRY = parseInt(process.env.FRL_C28_ENTRY || "0x80000", 16);
function runApp() {
  s.target.halt();
  if (Number(s.registers.read("PC")) >= 0x3F0000) s.registers.write("PC", ENTRY);
  s.target.run(false);
}
try {
  if (op === "identity") {
    const w = s.memory.read(0x5D008, 4, 16).map(Number);       // PARTIDL, PARTIDH: four 16-bit words
    console.log(`ID ${((w[3] << 16) | w[2]) >>> 0} ${((w[1] << 16) | w[0]) >>> 0}`);
    runApp();
  } else if (op === "backup") {
    s.memory.saveBinary(num(a[2]), num(a[3]), a[4]);
    console.log(`OK saved ${a[4]}`);
  } else if (op === "program") {
    s.target.reset("System Reset");
    const t0 = Date.now();
    s.memory.loadProgram(a[2]);
    console.log(`OK programmed in ${(Date.now() - t0) / 1000} s`);
    runApp();                                                  // no reset: it would park the chip in the boot ROM (2026-10-24)
  } else if (op === "restore") {
    s.target.reset("System Reset");
    s.memory.loadBinaryProgram(a[2], num(a[3]));
    s.memory.verifyBinaryProgram(a[2], num(a[3]));
    s.memory.saveBinary(num(a[3]), num(a[4]), a[5]);
    console.log(`OK restored and read back to ${a[5]}`);
    runApp();
  } else if (op === "verify") {
    try { s.memory.verifyProgram(a[2]); console.log("OK verified"); } catch (e) { console.log(`MISMATCH ${e.message}`); }
    runApp();
  } else if (op === "sample") {
    const dat = num(a[2]), dir = num(a[3]), bit = num(a[4]), ms = num(a[5]) * 1000;
    runApp();
    const settle = Date.now(); while (Date.now() - settle < 300) {}   // let the program set its pins up
    console.log(`E ${(Number(s.memory.readOne(dir, 32)) >>> bit) & 1}`);
    const t0 = Date.now();
    let last = -1, n = 0;
    while (Date.now() - t0 < ms) {
      const b = (Number(s.memory.readOne(dat, 32)) >>> bit) & 1;
      n++;
      if (b !== last) { console.log(`T ${Date.now() - t0} ${b}`); last = b; }
    }
    console.log(`N ${n} ${Date.now() - t0}`);
  } else {
    throw new Error(`unknown op ${op}`);
  }
} finally {
  s.target.disconnect();
  ds.shutdown();
}
