// Load the three-ways image into RAM on the LAUNCHXL-F2800137, run it free for <seconds>, halt, disconnect.
// Prints RUNNING when the program starts so a script can start the serial capture.
//   run.sh run_board.js <image.out> <ccxml> <seconds>
const i = process.argv.findIndex(x => x.endsWith("run_board.js"));
const [prog, ccxml, secs] = process.argv.slice(i + 1, i + 4);
const ds = initScripting();
ds.setScriptingTimeout(60000);
ds.configure(ccxml);
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
s.target.reset();
s.memory.loadProgram(prog);
s.target.run(false);
console.log("RUNNING");
const t0 = Date.now();
while (Date.now() - t0 < Number(secs) * 1000) {}
s.target.halt();
s.target.disconnect();
ds.shutdown();
