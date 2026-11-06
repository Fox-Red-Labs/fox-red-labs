// Load a program into the F28P65x over the onboard XDS110 and check it, in one debug session.
//   run.sh c2krun_f28p65x.js <prog.out> [mode] [args]
//   mode "blink" (default): run, sample GPADAT 8 times 250 ms apart, print GPIO12 (LED4) state.
//   mode "bench": run until the program halts at a breakpoint on symbol bench_done, then print
//                 the variables named in args (comma list), each read as 32-bit unsigned.
//                 Also paints the .stack section (from bench.map beside the .out) with 0xA5A5 before
//                 running and prints stack_used_words (the C28x stack grows up).
//                 Arg forms: <var>, log:<sym>:<words> (char text), f32:<sym|0xaddr>:<count>:<file> (floats to JSON).
const { join } = require("path");
const a = process.argv.slice(process.argv.findIndex(x => x.endsWith(".out")));
const prog = a[0], mode = a[1] || "blink", args = a[2] || "";
const ds = initScripting();
ds.setScriptingTimeout(120000);
ds.configure(join(__dirname, "F28P650DK9_CL650001.ccxml"));
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
s.target.reset();
s.memory.loadProgram(prog);
if (mode === "blink") {
  s.target.run(false);
  const sleep = (ms) => { const t = Date.now(); while (Date.now() - t < ms) {} };
  const out = [];
  for (let i = 0; i < 8; i++) { sleep(250); out.push(Number(s.memory.readOne(0x7F00, 32))); }
  console.log(out.map(v => `GPIO12=${(v >> 12) & 1}`).join("\n"));
  s.target.halt();
} else {
  let stk = null;
  try {
    const map = require("fs").readFileSync(prog.replace(/[^/]+\.out$/, "bench.map"), "utf8");
    const m = map.match(/^\.stack\s+\d+\s+([0-9a-f]{8})\s+([0-9a-f]{8})/m);
    if (m) { stk = [parseInt(m[1], 16), parseInt(m[2], 16)]; s.memory.fill(stk[0], stk[1], 0xA5A5, 16); }
  } catch (e) { stk = null; }
  s.breakpoints.add("bench_done");
  const t0 = Date.now();
  s.target.run();
  console.log(`host_ms=${Date.now() - t0}`);
  if (stk) {
    const w = s.memory.read(stk[0], stk[1], 16).map(Number);
    let top = w.length; while (top > 0 && w[top - 1] === 0xA5A5) top--;
    console.log(`stack_used_words=${top} of ${stk[1]}`);
  }
  for (const name of args.split(",").filter(Boolean)) {
    if (name.startsWith("f32:")) {          // f32:<symbol or address>:<count>:<file> saves 32-bit floats as JSON
      const [, sym, n, file] = name.split(":");
      const addr = /^0x/.test(sym) ? parseInt(sym, 16) : s.expressions.evaluate(sym.replace(/^\*/, ""));  // *var: the address is var's value
      const w = s.memory.read(addr, Number(n), 32).map(Number);
      const b = Buffer.alloc(4);
      require("fs").writeFileSync(file, JSON.stringify(w.map(v => { b.writeUInt32LE(v >>> 0); return b.readFloatLE(0); })));
      console.log(`${sym}: ${n} floats saved to ${file}`);
    } else if (name.startsWith("log:")) {          // log:<symbol>:<words> prints a C28x char array as text
      const [, sym, n] = name.split(":");
      const w = s.memory.read(s.expressions.evaluate(sym), Number(n), 16).map(Number);
      const end = w.indexOf(0);
      console.log(String.fromCharCode(...w.slice(0, end < 0 ? w.length : end)));
    } else {
      console.log(`${name}=${s.expressions.evaluate(name)}`);
    }
  }
}
s.target.disconnect();
ds.shutdown();
