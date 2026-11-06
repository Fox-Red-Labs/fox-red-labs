// Reset the F2800137 through the debugger and let it boot from flash (TI's demo), then disconnect.
const { join } = require("path");
const ds = initScripting();
ds.setScriptingTimeout(60000);
ds.configure(join(__dirname, "F2800137_CL371081.ccxml"));
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
s.target.reset();
s.target.run(false);
s.target.disconnect();
ds.shutdown();
