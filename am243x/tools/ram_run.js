// Run an image on the LP-AM243x R5F core 0 with the boot switches left as shipped (QSPI boot): the flash firmware has already
// started the device manager, so stop the core, reset only that core, write the ELF's loadable segments and start it.
// memory.loadProgram hangs in this state; direct segment writes do not. RAM only: nothing is written to flash.
// Run: /Applications/ti/ccs2101/ccs/scripting/run.sh am243x/tools/ram_run.js <image.out>
// AM243X_BARE=1: for images that set up no memory protection of their own (Zephyr, which normally inherits it from TI's bootloader):
// start through a stub in BTCM that programs the MPU and turns the caches on, as the bootloader would, then jumps to the entry point.
const fs = require("fs"), os = require("os"), { join } = require("path");
const image = process.argv[process.argv.length - 1];
const d = fs.readFileSync(image);
const ds = initScripting();
ds.setScriptingTimeout(120000);
const { cores } = ds.configure(join(__dirname, "..", "AM243x_LP_" + (process.env.AM243X_SERIAL || "S24L0096") + ".ccxml"));
const s = ds.openSession(cores.find(c => new RegExp(process.env.AM243X_CORE || "Cortex_R5_0_0").test(c)));
s.target.connect();
s.target.halt();
s.target.reset("CPU Reset");
try { s.target.halt(); } catch (e) {}
// Whichever program ran before may have left its tick timer (TIMER8) running with interrupts on, and interrupts open at the
// controller; a new program that only expects its own timer bits then sits in the interrupt handler for ever. Stop it and close them.
if (!/M4/.test(process.env.AM243X_CORE || "")) {  // the M4F has no such interrupt controller and a different memory map: only the R5Fs get this
  s.memory.write(0x2480038, [0], 32);
  s.memory.write(0x2480030, [7], 32);
  s.memory.write(0x2480028, [7], 32);
  for (let g = 0; g < 16; g++) s.memory.write(0x2fff040c + 0x20 * g, [0xffffffff], 32);
  for (let i = 0; i < 16; i++) s.memory.write(0x2fff0018, [0], 32);
}
const phoff = d.readUInt32LE(28), phsize = d.readUInt16LE(42), phnum = d.readUInt16LE(44), entry = d.readUInt32LE(24);
const tmp = join(os.tmpdir(), "frl-am243x-seg.bin");
for (let i = 0; i < phnum; i++) {
  const p = phoff + i * phsize;
  if (d.readUInt32LE(p) !== 1) continue;
  const off = d.readUInt32LE(p + 4), addr = d.readUInt32LE(p + 8), fsz = d.readUInt32LE(p + 16);
  if (!fsz) continue;
  fs.writeFileSync(tmp, d.subarray(off, off + fsz));
  s.memory.loadBinary(addr, tmp);
  console.log("FRL_SEG", addr.toString(16), fsz);
}
if (process.env.AM243X_BARE === "1") {
  const movw = (r, v) => [0xe3000000 | ((v >>> 12 & 15) << 16) | (r << 12) | (v & 0xfff), 0xe3400000 | ((v >>> 28) << 16) | (r << 12) | (v >>> 16 & 0xfff)];
  const code = [];
  // region number, base, size register (enable + log2 size), access: 4 GB strongly ordered and not executable, then normal write-back RAM
  const regions = [[0, 0, 0x3f, 0x1304], [1, 0, 0x1d, 0x30b], [2, 0x41010000, 0x1d, 0x30b], [3, 0x70000000, 0x29, 0x30b]];
  for (const [n, base, size, access] of regions) {
    code.push(...movw(0, n), 0xee060f12);        // RGNR
    code.push(...movw(0, base), 0xee060f11);     // region base
    code.push(...movw(0, size), 0xee060f51);     // region size and enable
    code.push(...movw(0, access), 0xee060f91);   // region access
  }
  code.push(...movw(0, 0), 0xee070f15, 0xee0f0f15, 0xf57ff04f); // ICIALLU, DCIALL (discard), dsb
  code.push(0xee112f10, 0xe3822005, 0xe3822a01, 0xee012f10, 0xf57ff06f, 0xe12fff11); // SCTLR |= M, C, I; isb; bx r1
  const stub = Buffer.alloc(code.length * 4);
  code.forEach((w, i) => stub.writeUInt32LE(w >>> 0, i * 4));
  fs.writeFileSync(tmp, stub);
  s.memory.loadBinary(0x41010000, tmp);
  fs.unlinkSync(tmp);
  s.registers.write("R1", entry);
  s.registers.write("CPSR", 0x1d3);
  s.registers.write("PC", 0x41010000);
} else {
  fs.unlinkSync(tmp);
  s.registers.write("PC", entry);
}
let cpsr = "-"; try { cpsr = s.registers.read("CPSR").toString(16); } catch (e) {}  // the M4F has xPSR, not CPSR
console.log("FRL_START pc", s.registers.read("PC").toString(16), "cpsr", cpsr);
s.target.run(false);
console.log("FRL_RUN");
ds.shutdown();
