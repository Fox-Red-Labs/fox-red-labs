// Read-only: ask a CC27xx's boot ROM for its HSM system information over SACI, then let it boot again.
// Writes no flash. It does reset the chip twice: TI's EnterSaci() (SWD reset into SACI) and SACI_CMD_BLDR_APP_RESET_DEVICE at the end.
//   /Applications/ti/ccs2101/ccs/scripting/run.sh <repo>/cc2755p10/recovery/saci_info.js <absolute ccxml>
// HSM_GET_SYS_INFO (0x22) format: TRM SWCU195A 9.3.2.1.5. Word 1: firmware patch, minor, major, rollback ID; word 2: hardware version.
const ccxml = process.argv[process.argv.findIndex(x => x.endsWith("saci_info.js")) + 1];
const hex = v => "0x" + (v >>> 0).toString(16).padStart(8, "0");
const ds = initScripting(); ds.setScriptingTimeout(60000);
ds.configure(ccxml);
const s = ds.openSession(/CS_DAP/); s.target.connect();
const rd = r => Number(BigInt.asUintN(32, BigInt(s.expressions.evaluate("'REG'::" + r))));
const wr = (r, v) => s.expressions.evaluate("'REG'::" + r + " = 0x" + (v >>> 0).toString(16));
const waitTx = () => { const t = Date.now(); while (rd("SECAP_TXCTL") & 1) if (Date.now() - t > 2000) throw new Error("TXD_FULL timeout"); };
const waitRx = () => { const t = Date.now(); while (!(rd("SECAP_RXCTL") & 1)) if (Date.now() - t > 5000) throw new Error("no response, RXCTL " + hex(rd("SECAP_RXCTL"))); };
let seq = 0x60;
function cmd(id, resp = true) {
  const w0 = (id | ((seq++ & 0xff) << 8)) >>> 0; waitTx(); wr("SECAP_TXCTL", 2); wr("SECAP_TXD", w0); waitTx();
  if (!resp) return null;
  waitRx(); const h = rd("SECAP_RXD"); const d = []; for (let i = 0; i < (h >>> 24); i++) { waitRx(); d.push(rd("SECAP_RXD")); }
  console.log("cmd 0x" + id.toString(16), "hdr", hex(h), "result 0x" + ((h >>> 16) & 0xff).toString(16), d.map(hex).join(" "));
  return { r: (h >>> 16) & 0xff, d };
}
console.log("before DEVICESTATUS", hex(rd("CFGAP_DEVICESTATUS")));
const m = ds.openSession(/Cortex_M33_0/); m.expressions.evaluate("EnterSaci()");
console.log("in SACI DEVICESTATUS", hex(rd("CFGAP_DEVICESTATUS")));
cmd(0x01);
const h = cmd(0x22);
if (h.r === 0) { const f = h.d[0], w = h.d[1];
  console.log("HSM firmware " + ((f >>> 16) & 0xff) + "." + ((f >>> 8) & 0xff) + "." + (f & 0xff) + ", rollback ID " + (f >>> 24) +
    "; HSM hardware " + ((w >>> 16) & 0xff) + "." + ((w >>> 8) & 0xff) + "." + (w & 0xff)); }
else console.log(h.r === 0x8e ? "HSM_BOOT_FAILED" : "HSM info result 0x" + h.r.toString(16));
cmd(0x23);
cmd(0x14, false); // BLDR_APP_RESET_DEVICE, waitForSwdDisconnect = 0: SACI times out and the chip boots (no response by design)
console.log("reset sent; read the boot status with tools/recovery/cc2x_status.js");
s.target.disconnect(); ds.shutdown();
