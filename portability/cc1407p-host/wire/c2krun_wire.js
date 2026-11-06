// Load the wire packer test (build_c28x.sh) into RAM on the LAUNCHXL-F2800137, run it, halt after 2 s, and print the
// result globals and every packed octet (one hex pair each), for comparison with the host's stream.bin.
//   run.sh c2krun_wire.js <wire_c28x.out> <ccxml> <out.txt>
const fs = require("fs");
const i = process.argv.findIndex(x => x.endsWith("c2krun_wire.js"));
const [prog, ccxml, outp] = process.argv.slice(i + 1, i + 4);
const ds = initScripting();
ds.setScriptingTimeout(60000);
ds.configure(ccxml);
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
s.target.reset();
s.memory.loadProgram(prog);
// run free, wait, halt: the test parks in a loop after wire_run (a breakpoint on the empty wire_halt did not return)
s.target.run(false);
const t0 = Date.now();
while (Date.now() - t0 < 2000) {}
s.target.halt();
const ms = Date.now() - t0;
const addr = (sym) => s.expressions.evaluate("&" + sym);
const rd = (sym, n, bits) => s.memory.read(addr(sym), n, bits || 16).map(Number);
const cases = rd("wire_cases", 1)[0], fail = rd("wire_fail", 1)[0], n = rd("wire_out_n", 1)[0], done = rd("wire_done", 1)[0];
const maskLo = rd("wire_fail_mask_lo", 1)[0], maskHi = rd("wire_fail_mask_hi", 1)[0];
const dg = rd("wire_digest", 2); const digest = ((dg[1] << 16) | dg[0]) >>> 0;
const out = rd("wire_out", n).map(v => (v & 0xFF).toString(16).padStart(2, "0")).join("");
fs.writeFileSync(outp, `run_ms ${ms}\ndone ${done}\ncases ${cases}\nfail ${fail}\nmask ${maskHi.toString(16)} ${maskLo.toString(16)}\noctets ${n}\ndigest ${digest.toString(16).toUpperCase().padStart(8, "0")}\nstream ${out}\n`);
console.log(`done ${done} cases ${cases} fail ${fail} octets ${n} digest ${digest.toString(16).toUpperCase()} run_ms ${ms}`);
s.target.halt();
s.target.disconnect();
ds.shutdown();
