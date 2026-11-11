// The Digital Lab page's own chrome: the three-step tracker and the "Take it home" kit.
// It only reads what bench.js already puts in the page (data-state attributes, the pressed demo, the board name); it
// never touches a device. The kit's markup and copy logic match tools/site_kit.py (KIT_JS); kit.json and the generated
// blocks of index.html come from tools/webflash/build_ui.py.
(() => {
  const $ = (id) => document.getElementById(id);
  const params = new URLSearchParams(location.search);
  let picked = !!params.get("recipe");

  // ---- the "What it does" note: open on a computer, folded on a phone
  const cap = $("cap-box");
  if (cap && window.matchMedia && window.matchMedia("(max-width:620px)").matches) cap.open = false;

  // ---- where you are
  function track() {
    const scene = $("scene"), res = $("result-card"), detail = $("detail");
    const connected = scene && scene.dataset.state && scene.dataset.state !== "idle";
    const rs = res ? res.dataset.state : "";
    const st = { connect: connected ? "done" : "now", pick: "todo", watch: "todo" };
    if (connected) {
      st.pick = picked ? "done" : "now";
      if (picked) st.watch = rs === "pass" ? "done" : rs === "fail" ? "bad" : "now";
    }
    document.querySelectorAll("#stepper li").forEach((li) => {
      li.dataset.st = st[li.dataset.step];
      if (st[li.dataset.step] === "now") li.setAttribute("aria-current", "step"); else li.removeAttribute("aria-current");
    });
    const mark = (el, on) => el && el.classList.toggle("now", !!on);
    mark(scene, st.connect === "now"); mark($("pick"), st.pick === "now"); mark(detail, st.watch === "now");
    if (rs === "pass") showHome();
  }

  // ---- once a board is connected the big Connect section is one line (board, serial, Switch)
  function compact(force) {
    const sc = $("scene"), bar = $("conn-bar"); if (!sc || !bar) return;
    const on = sc.dataset.state && sc.dataset.state !== "idle" && $("found") && $("found").dataset.state === "on";
    const open = on && force !== "open" && !bar.dataset.open;
    sc.classList.toggle("compact", !!open); document.body.classList.toggle("connected", !!on);
    bar.hidden = !on;
    if (on) { $("cb-kit").textContent = ($("scene-kit").textContent || "").trim().replace(/^LP-/i, "LP-"); const s = document.body.dataset.probe; $("cb-serial").textContent = s ? `XDS110 ${s}` : "connected"; }
  }
  if ($("cb-switch")) $("cb-switch").addEventListener("click", () => { const bar = $("conn-bar"); bar.dataset.open = bar.dataset.open ? "" : "1"; compact(); $("scene").scrollIntoView({ block: "nearest" }); });
  document.addEventListener("frl-terminal", () => compact());

  // ---- the Take it home block starts folded; its head, or the stage's "Run the full version", opens it
  const th = $("takehome");
  if (th) {
    th.classList.add("folded");
    const head = th.querySelector(".head");
    if (head) head.addEventListener("click", () => th.classList.toggle("folded"));
    const full = $("c-full");
    if (full) full.addEventListener("click", (e) => { e.preventDefault(); th.classList.remove("folded"); th.scrollIntoView({ behavior: "smooth", block: "start" }); });
  }

  // ---- after a result: the way back, in view, on boards where this page can put the saved copy back
  const pbRow = $("r-putback"), pbBtn = $("r-putback-btn"), res = $("result"), safe = $("safe");
  if (pbRow && pbBtn && res) {
    const sync = () => { pbRow.hidden = res.classList.contains("hidden") || !safe || safe.dataset.tone !== "safe"; };
    new MutationObserver(sync).observe(res, { attributes: true, attributeFilter: ["class"] });
    if (safe) new MutationObserver(sync).observe(safe, { attributes: true, attributeFilter: ["data-tone"] });
    sync();
    // Opens "Put back what was there" with the copy saved this visit already chosen (the same checks run as for a
    // file picked by hand); the person still presses the put-back button there.
    pbBtn.addEventListener("click", () => {
      const more = document.querySelector("section.more"), d = more && more.querySelector("details");
      if (!d) return;
      d.open = true;
      const c = window.frlLastCopy, input = $("file");
      if (c && c.json && input && window.DataTransfer) {
        try {
          const dt = new DataTransfer();
          dt.items.add(new File([c.json], c.file, { type: "application/json" }));
          input.files = dt.files;
          input.dispatchEvent(new Event("change", { bubbles: true }));
        } catch (e) { /* the person can still choose the file from Downloads */ }
      }
      d.scrollIntoView({ behavior: "smooth", block: "start" });
    });
  }

  // ---- take it home
  let kitData = null, os = /Win/.test(navigator.platform) ? "win" : /Linux/.test(navigator.platform) ? "linux" : "mac";
  const k = () => document.querySelector("#takehome #th-ai");
  const osButtons = () => document.querySelectorAll("#home-kit .k-os button");
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
  function board() {
    const name = ($("scene-kit") && $("scene-kit").textContent || "").trim();
    const boards = kitData.boards, hit = Object.keys(boards).find((x) => x.toUpperCase() === name);
    if (hit) return [hit, boards[hit]];
    const sel = $("board-preview"), id = sel && sel.value;
    const byId = Object.keys(boards).find((x) => boards[x].id === id);
    return byId ? [byId, boards[byId]] : [Object.keys(boards)[0], boards[Object.keys(boards)[0]]];
  }
  function text(root) {
    const parts = [];
    root.querySelectorAll("input:checked").forEach((i) => { const t = i.getAttribute("data-p-" + os) || i.getAttribute("data-p-all"); if (t) parts.push(t); });
    return parts.join(" ");
  }
  function preview() { const r = k(); if (r) r.querySelector(".k-prev").textContent = text(r); }
  function rows() {
    if (!kitData || !k()) return;
    const [kit, b] = board(), r = k();
    const was = {}; r.querySelectorAll("label").forEach((l) => { const i = l.querySelector("input"); was[l.querySelector("span").firstChild.textContent] = i.checked; });
    const demo = $("r-title") && $("r-title").textContent.trim();
    const opts = b.options.slice();
    if (demo) {
      const tpl = b.folder ? kitData.demo.text : kitData.demo.text_nofolder;
      const probe = /^[A-Za-z0-9]{1,16}$/.test(document.body.dataset.probe || "") ? document.body.dataset.probe : "";
      opts.push({ label: kitData.demo.label, sub: demo, texts: { all: tpl.replace("{title}", demo).replace("{kit}", kit).replace("{folder}", b.folder) + (probe ? ` My XDS110 probe's serial number, as the browser read it through the debugger, is ${probe}; if frl detect lists a different serial for the same board, that one is the USB serial to use.` : "") }, on: true });
    }
    r.querySelectorAll("label").forEach((l) => l.remove());
    const anchor = r.querySelector(".k-prev-h");
    opts.forEach((o) => {
      const on = o.locked || (o.label in was ? was[o.label] : o.on);
      const attrs = Object.keys(o.texts).map((x) => ` data-p-${x}="${esc(o.texts[x])}"`).join("");
      const l = document.createElement("label");
      l.innerHTML = `<input type="checkbox"${attrs}${on ? " checked" : ""}${o.locked ? " disabled" : ""}><span>${esc(o.label)}<small>${esc(o.sub)}</small></span>`;
      l.querySelector("input").addEventListener("change", preview);
      anchor.parentNode.insertBefore(l, anchor);
    });
    preview();
  }
  function showHome() { if (kitData) rows(); }
  // Copy for your terminal: bench.js writes the lines for every computer into the block; this shows the one picked.
  function showTerm() {
    const pre = $("th-term-pre"); if (!pre || !pre.dataset.t) return;
    try { pre.textContent = JSON.parse(pre.dataset.t)[os] || ""; const n = $("th-term-note"); if (n) n.textContent = (JSON.parse(pre.dataset.n || "{}")[os]) || ""; } catch (e) { /* the block stays as it was */ }
  }
  function copy(b, text, ok) {
    const done = () => { const o = b.dataset.label || b.textContent; b.dataset.label = o; b.textContent = ok; setTimeout(() => { b.textContent = o; }, 2000); };
    if (navigator.clipboard) navigator.clipboard.writeText(text).then(done).catch(() => {});
  }
  function wireKit() {
    const r = k(); if (!r) return;
    osButtons().forEach((ob) => {
      ob.setAttribute("aria-pressed", ob.dataset.os === os);
      ob.addEventListener("click", () => { os = ob.dataset.os; osButtons().forEach((x) => x.setAttribute("aria-pressed", x === ob)); preview(); showTerm(); });
    });
    const ai = $("th-ai-copy"), term = $("th-term-copy");
    if (ai) ai.addEventListener("click", () => copy(ai, text(r), "Copied. Paste it into your assistant"));
    if (term) term.addEventListener("click", () => copy(term, $("th-term-pre").textContent, "Copied. Paste it into your terminal"));
    document.addEventListener("frl-terminal", () => { showTerm(); rows(); });
    showTerm();
  }

  fetch("kit.json").then((x) => x.json()).then((d) => { kitData = d; wireKit(); rows(); }).catch(() => {});

  const watchFound = () => { if ($("found")) new MutationObserver(() => compact()).observe($("found"), { attributes: true, attributeFilter: ["data-state"] }); };
  const watch = (el, opts) => el && new MutationObserver(track).observe(el, opts);
  document.addEventListener("DOMContentLoaded", () => {});
  watch($("scene"), { attributes: true, attributeFilter: ["data-state"] }); watchFound(); watch($("scene"), { attributes: true, attributeFilter: ["data-state"] }); new MutationObserver(() => compact()).observe($("scene"), { attributes: true, attributeFilter: ["data-state"] });
  watch($("result-card"), { attributes: true, attributeFilter: ["data-state"] });
  watch($("recipes"), { attributes: true, subtree: true, attributeFilter: ["aria-pressed"] });
  watch($("ai-models"), { attributes: true, subtree: true, attributeFilter: ["aria-pressed"] });
  for (const id of ["recipes", "ai-models"]) if ($(id)) $(id).addEventListener("click", (e) => { if (e.target.closest(".recipe:not(:disabled):not([data-ai-picker])")) { picked = true; track(); if (kitData && !$("takehome").classList.contains("hidden")) rows(); } });
  if ($("connect")) $("connect").addEventListener("click", () => setTimeout(track, 0));
  track();
})();
