// Read a board's backchannel serial port (the second serial port of an XDS110) for a few seconds: the Node twin of the Talk tab's Web Serial read.
//   node tools/webflash/node-console.mjs --probe LS5600RE --baud 115200 --seconds 6
// Needs serialport in NODE_PATH (like node-run.mjs needs node-hid). Take the probe lock first when a flash may follow.
import { createRequire } from "module";
import { join, dirname } from "path";
import { fileURLToPath } from "url";
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const require = createRequire(join(process.env.NODE_PATH || ROOT, "x.js"));
const { SerialPort } = require("serialport");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const serial = arg("--probe"), baud = +arg("--baud", 115200), secs = +arg("--seconds", 6);
const ports = await SerialPort.list();
const p = ports.find((x) => x.serialNumber === serial && /1$/.test(x.path)) || ports.find((x) => x.path.endsWith(serial + "1"));
if (!p) { console.log("no serial port for probe " + serial + "; ports: " + ports.map((x) => x.path).join(" ")); process.exit(2); }
const sp = new SerialPort({ path: p.path, baudRate: baud });
let text = "";
sp.on("data", (d) => { text += d.toString("latin1"); });
await new Promise((r) => sp.on("open", r));
await new Promise((r) => setTimeout(r, secs * 1000));
await new Promise((r) => sp.close(r));
console.log(`# ${p.path} at ${baud} baud, ${secs} s`);
console.log(text.replace(/\r/g, ""));
