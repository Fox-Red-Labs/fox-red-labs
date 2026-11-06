// Wait-for-debug connect, erase MAIN only, write the saved image at 0x10000000, read 1 MB of main flash back.
const a = process.argv; const i = a.findIndex(x => x.endsWith("restore.js"));
const [ccxml, bin, out, mode] = a.slice(i + 1, i + 5);
const ds = initScripting(); ds.setScriptingTimeout(900000);
ds.configure(ccxml);
const m = ds.openSession(/CORTEX_M33/);
m.settings.set("EnableWaitForDebug", true);
m.target.connect();
if (!m.target.isHalted()) m.target.halt();
console.log("connected, PC 0x" + m.registers.read("PC").toString(16));
if (mode === "write") {
  m.settings.set("FlashEraseSelection", "Erase MAIN memory only");
  m.settings.set("FlashResetAfterLoadSetting", false);
  console.log("erase:", m.settings.get("FlashEraseSelection"), "verify:", m.settings.get("FlashVerification"));
  const t = Date.now();
  m.memory.loadBinaryProgram(bin, 0x10000000n);
  console.log("loaded in", ((Date.now() - t) / 1000).toFixed(1), "s");
}
const t2 = Date.now();
m.memory.saveBinary(0x10000000n, 262144, out);
console.log("read back in", ((Date.now() - t2) / 1000).toFixed(1), "s");
m.target.disconnect(); ds.shutdown();
