// "Use your own tools" in Take it home: the demo's image file (an ELF, firmware.out) and the command that writes it
// with the tool frl uses for that family. Reads window.frlChosen (set by bench.js when a demo is picked).
(function () {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const box = $("th-own");
  if (!box) return;
  const OPENOCD = (cfg, which) => ({
    cmd: `openocd -f interface/xds110.cfg -f target/${cfg} \\\n  -c "program firmware.out verify reset exit"`,
    note: `${which}. Run it in the folder where you saved firmware.out, with only this board's probe plugged in (or add -c "adapter serial <probe serial>" before the target line).`,
  });
  const FAMILY = [
    [/^mspm0/, () => OPENOCD("ti_mspm0.cfg", "xPack OpenOCD 0.12.0-7 or newer (it has target/ti_mspm0.cfg); checked with this blink on an LP-MSPM0L1306")],
    [/^mspm33/, () => OPENOCD("ti_mspm33.cfg", "TI's OpenOCD (CCSTUDIO-OPENOCD 1.5.0); xPack OpenOCD has no MSPM33 driver")],
    [/^am13/, () => OPENOCD("ti_am13e230x.cfg", "TI's OpenOCD (CCSTUDIO-OPENOCD 1.5.0)")],
    [/^cc2[37]/, () => ({ cmd: "dslite.sh --mode flash -c <your board's .ccxml> -e -f -v firmware.out",
      note: "TI's DSLite, inside UniFlash or CCStudio. TI's tools erase the whole chip on this family, its configuration area included: keep the copy this page saved." })],
  ];
  function render() {
    const c = window.frlChosen;
    const fam = c && FAMILY.find(([rx]) => rx.test(c.board || ""));
    box.hidden = !c || !c.image || !fam;
    if (box.hidden) return;
    const f = fam[1]();
    const img = $("th-own-img");
    img.textContent = "";
    const a = document.createElement("a");
    a.href = c.image; a.download = "firmware.out"; a.textContent = "Download the image (firmware.out, an ELF)";
    img.append(a, document.createTextNode(` ${c.bytes} bytes of program, sha256 ${String(c.sha256).slice(0, 12)}...`));
    $("th-own-cmd").textContent = f.cmd;
    $("th-own-note").textContent = f.note;
  }
  let last = "";
  setInterval(() => { const k = JSON.stringify(window.frlChosen || null); if (k !== last) { last = k; render(); } }, 500);
})();
