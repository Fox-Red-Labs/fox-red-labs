// Read the F2800137's main flash (0x20000 words from 0x80000) to a file. Read-only: no erase, no program.
//   run.sh c2kbackup.js <out .bin>
const { join } = require("path");
const out = process.argv[process.argv.findIndex(x => x.endsWith("c2kbackup.js")) + 1];
const ds = initScripting();
ds.setScriptingTimeout(120000);
ds.configure(join(__dirname, "F2800137_CL371081.ccxml"));
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
s.memory.saveBinary(0x80000, 0x20000, out);
console.log(`saved ${out}`);
s.target.disconnect();
ds.shutdown();
