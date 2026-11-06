// Put the F2800137's flash back exactly as it arrived (TI's out-of-box demo), check it, and start it.
//   run.sh c2kflashback.js <flash image .bin> <readback .bin>
// Writes the saved 256 kB image (data/flash-as-received.bin, read from 0x80000) through the debugger's
// flash tool (main flash only: erase and program; no OTP or security operation is called), verifies it
// with verifyBinaryProgram, reads the whole bank back to <readback .bin> for a hash check, then does a
// System Reset and lets the chip boot from flash. Probe CL371081 only (see the connection file).
const { join } = require("path");
const a = process.argv.slice(process.argv.findIndex(x => x.endsWith(".bin")));
const img = a[0], back = a[1];
const ds = initScripting();
ds.setScriptingTimeout(600000);
ds.configure(join(__dirname, "F2800137_CL371081.ccxml"));
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
s.target.reset("System Reset");
const t0 = Date.now();
s.memory.loadBinaryProgram(img, 0x80000);
console.log(`programmed in ${(Date.now() - t0) / 1000} s`);
s.memory.verifyBinaryProgram(img, 0x80000);
console.log("verifyBinaryProgram: ok");
s.memory.saveBinary(0x80000, 0x20000, back);
console.log(`read back to ${back}`);
s.target.reset("System Reset");
s.target.run(false);
s.target.disconnect();
ds.shutdown();
