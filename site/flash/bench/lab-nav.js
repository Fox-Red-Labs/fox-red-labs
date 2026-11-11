// The Digital Lab's left rail (desktop, wider than 900 px): one item per job on the page, with a one-line state.
// It only reads the page and scrolls or opens what is already there; it writes nothing to a board.
// Items follow docs/research/digital-lab-feature-survey-2026-10-25.md; Memory joins when the memory view exists.
(function () {
  "use strict";
  const $ = (s) => document.querySelector(s);
  const shown = (s) => { const e = $(s); return !!e && !e.hidden && !e.classList.contains("hidden") && e.offsetParent !== null; };
  const text = (s) => (shown(s) ? ($(s).textContent || "").trim() : "");
  const demo = () => shown("#detail");
  const tabState = (id, what) => () => {
    if (!demo()) return { off: true, sub: "Pick a demo first" };
    const t = $(id);
    if (!t || t.hidden || t.disabled) return { off: true, sub: `This demo has no ${what}` };
    return { off: false, sub: what === "console" ? "Serial in and out" : "Values the page read" };
  };
  const ITEMS = [
    { key: "board", target: "#scene", n: "1", label: "Board", step: "connect",
      state: () => ({ sub: text("#cb-kit") || "Not connected" }) },
    { key: "demos", target: "#pick", n: "2", label: "Demos", step: "pick",
      state: () => ({ sub: text("#r-title") || "Pick one to send" }) },
    { key: "watch", target: "#detail", n: "3", label: "Watch it work", step: "watch", tab: "#t-see",
      state: () => (demo() ? { sub: "Send it, then read the result" } : { off: true, sub: "Pick a demo first" }) },
    { key: "console", target: "#detail", label: "Console", tab: "#t-talk", state: tabState("#t-talk", "console") },
    { key: "data", target: "#detail", label: "Live values", tab: "#t-live", state: tabState("#t-live", "live values") },
    { key: "chip", href: "../chip/", label: "Inside the chip", state: () => ({ sub: "Every block, live, while it runs" }) },
    { key: "pages", href: "../../index.html#drive", label: "Demo pages", state: () => ({ sub: "Motor console, radar room, dashboards" }) },
    { key: "backup", target: "section.more", label: "Backup", open: true,
      state: () => ({ sub: "Put a saved copy back" }) },
    { key: "home", target: "#takehome", label: "Take it home", unfold: true, state: () => ({ sub: "Your computer, your tools" }) },
  ];

  // On another Lab page (body[data-lab-page]) the rail links back to the Lab and marks that page.
  const here = document.body.dataset.labPage || "";
  const main = $("main");
  if (!main || $("#lab-nav")) return;
  const nav = document.createElement("nav");
  nav.id = "lab-nav";
  nav.setAttribute("aria-label", "Digital Lab sections");
  const head = document.createElement("p");
  head.className = "ln-h";
  head.textContent = "Digital Lab";
  const ol = document.createElement("ol");
  nav.append(head, ol);
  const rows = new Map();
  for (const it of ITEMS) {
    const li = document.createElement("li");
    li.dataset.key = it.key;
    const a = document.createElement("a");
    a.href = it.href || (here ? "../bench/" + (it.target && it.target[0] === "#" ? it.target : "") : "#");
    const n = document.createElement("span");
    n.className = "ln-n";
    n.textContent = it.n || "";
    const t = document.createElement("span");
    t.className = "ln-t";
    const b = document.createElement("b");
    b.textContent = it.label;
    const small = document.createElement("small");
    t.append(b, small);
    a.append(n, t);
    li.append(a);
    ol.append(li);
    rows.set(it.key, { li, a, small });
  }
  const shell = document.createElement("div");
  shell.className = "lab-shell";
  main.parentNode.insertBefore(shell, main);
  const col = document.createElement("div");
  col.className = "lab-col";
  shell.append(nav, col);
  col.append(main);
  const more = $("section.more");
  if (more) col.append(more);

  let pinned = null, landing = false;   // see the scroll handlers below
  function refresh() {
    if (here) {
      for (const it of ITEMS) { const r = rows.get(it.key); if (r.small.textContent !== (it.key === here ? "You are here" : "Open in the Digital Lab")) r.small.textContent = it.key === here ? "You are here" : "Open in the Digital Lab"; }
      return;
    }
    for (const it of ITEMS) {
      const r = rows.get(it.key);
      const s = it.state();
      const off = !!s.off;
      if (r.li.classList.contains("off") !== off) r.li.classList.toggle("off", off);
      if (off) r.a.setAttribute("aria-disabled", "true"); else r.a.removeAttribute("aria-disabled");
      if (r.small.textContent !== s.sub) r.small.textContent = s.sub;
      const st = it.step ? ($(`#stepper li[data-step="${it.step}"]`) || {}).dataset : null;
      const v = st && st.st ? st.st : "";
      if ((r.li.dataset.st || "") !== v) r.li.dataset.st = v;
    }
    if (typeof mark === "function") mark();   // a section that opened or closed moves what is in view
  }
  let queued = false;
  new MutationObserver((ms) => {
    if (queued || ms.every((m) => nav.contains(m.target))) return;
    queued = true;
    requestAnimationFrame(() => { queued = false; refresh(); });
  }).observe(document.body, { subtree: true, childList: true, attributes: true, characterData: true });
  refresh();

  // Which item is in view: the section that starts closest above the upper third of the window.
  // The item just clicked stays marked while the page scrolls to it; any later scroll (the reader's, or the page
  // moving to a result) lets the position decide again.
  for (const ev of ["wheel", "touchmove", "keydown"]) window.addEventListener(ev, () => { pinned = null; }, { passive: true });
  window.addEventListener("scrollend", () => { if (landing) landing = false; else pinned = null; });
  function mark() {
    if (here) { for (const [k, r] of rows) { r.li.classList.toggle("on", k === here); if (k === here) r.a.setAttribute("aria-current", "page"); } return; }
    if (pinned) { for (const [k, r] of rows) r.li.classList.toggle("on", k === pinned); return; }
    const y = window.innerHeight * 0.33;
    let best = null, top = -Infinity;
    for (const it of ITEMS) {
      const el = it.target ? $(it.target) : null;
      if (!el || el.offsetParent === null) continue;
      const r = el.getBoundingClientRect();
      if (r.top <= y && r.bottom > 0 && r.top >= top) { best = it.key; top = r.top; }   // a tie goes to the later item (the demo stage sits beside the list)
    }
    if (best === "watch" || best === "console" || best === "data") {
      const open = ITEMS.find((it) => it.tab && $(it.tab) && $(it.tab).getAttribute("aria-selected") === "true");
      best = open ? open.key : "watch";
    }
    for (const [k, r] of rows) r.li.classList.toggle("on", k === best);
  }
  window.addEventListener("scroll", () => requestAnimationFrame(mark), { passive: true });
  mark();

  ol.addEventListener("click", (ev) => {
    const a = ev.target.closest("a");
    if (!a) return;
    const it = ITEMS.find((x) => x.key === a.parentElement.dataset.key);
    if (here || (it && it.href)) return;   // a real page: let the link open it
    ev.preventDefault();
    if (!it) return;
    if (a.parentElement.classList.contains("off")) { a.title = rows.get(it.key).small.textContent; return; }
    if (it.tab && $(it.tab)) $(it.tab).click();
    const el = $(it.target);
    if (!el) return;
    if (it.open) { const d = el.matches("details") ? el : el.querySelector("details"); if (d) d.open = true; }
    if (it.unfold) el.classList.remove("folded");
    pinned = it.key; landing = true;
    el.scrollIntoView({ behavior: "smooth", block: "start" });
    mark();
  });
})();
