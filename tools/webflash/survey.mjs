// Read-only survey of every XDS110 on USB through its CMSIS-DAP HID interface, with the browser bench's own code:
// probe firmware, SWD debug port ID, the core (CPUID), the memory port's ID, and the MSPM0-family factory region.
// Nothing is written to any board; nothing is halted.
//
//   node tools/webflash/survey.mjs [--json]
import { createRequire } from "module";
import { join, dirname } from "path";
import { fileURLToPath } from "url";
import { Dap } from "../../site/flash/mspm0-dap.js";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const require = createRequire(join(process.env.NODE_PATH || ROOT, "x.js"));
const HID = require("node-hid");
const hex = (v) => (v === null || v === undefined ? null : "0x" + (v >>> 0).toString(16).toUpperCase().padStart(8, "0"));
const CORES = { 0xC20: "Cortex-M0", 0xC60: "Cortex-M0+", 0xC23: "Cortex-M3", 0xC24: "Cortex-M4", 0xD21: "Cortex-M33", 0xD22: "Cortex-M55", 0xD20: "Cortex-M23" };

const out = [];
for (const d of HID.devices().filter((x) => x.vendorId === 0x451 && x.interface === 5)) {
  const dev = new HID.HID(d.path);
  const t = { open: async () => {}, close: async () => {}, transfer: async (b) => { const o = new Array(65).fill(0); b.forEach((x, i) => { o[i + 1] = x; }); dev.write(o); const r = dev.readTimeout(3000); if (!r.length) throw new Error("no reply"); return Uint8Array.from(r); } };
  const dap = new Dap(t);
  const row = { serial: d.serialNumber, product: d.product };
  try {
    row.dap_firmware = await dap.info(0x04);
    try {
      row.dpidr = hex(await dap.connect());
      row.ap0_idr = hex(await dap.apRead(0, 0xFC));
      const cpuid = await dap.read32(0xE000ED00).catch(() => null);
      row.cpuid = hex(cpuid);
      if (cpuid !== null) row.core = CORES[(cpuid >>> 4) & 0xFFF] || `part 0x${((cpuid >>> 4) & 0xFFF).toString(16)}`;
      const ids = [];
      for (let i = 0; i < 3; i++) ids.push(await dap.read32(0x41C40000 + 4 * i).catch(() => null));
      row.factory_traceid = hex(ids[0]); row.factory_deviceid = hex(ids[1]); row.factory_userid = hex(ids[2]);
      if (ids[1]) row.part = "0x" + ((ids[1] >>> 12) & 0xFFFF).toString(16).toUpperCase() + ", manufacturer 0x" + ((ids[1] >>> 1) & 0x7FF).toString(16).toUpperCase();
    } catch (e) {
      row.swd = "no answer: " + e.message;
    }
    await dap.disconnect();
  } catch (e) {
    row.error = e.message;
  } finally {
    dev.close();
  }
  out.push(row);
}
out.sort((a, b) => a.serial.localeCompare(b.serial));
if (process.argv.includes("--json")) console.log(JSON.stringify(out, null, 1));
else for (const r of out) console.log([r.serial, r.dap_firmware, r.dpidr || r.swd || r.error, r.core || "", r.factory_deviceid || "", r.part || ""].join(" | "));
