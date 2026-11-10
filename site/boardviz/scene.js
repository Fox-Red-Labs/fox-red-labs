// A demo scene on the drawn-board engine: boards that stack by mating their headers, wires that draw themselves
// between named places, switches and jumpers that move, LEDs that light and a motor that turns, driven by a step
// list beside the picture. Data: data/boardviz/scenes/<demo>.json (tools/boardviz/README.md says the shape).
import { BoardViz, buildBoard, PARTS, mat, T, COL } from "./boardviz.js";
import { HOBBY } from "./hobby-parts.js";

const ease = t => t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;

export class SceneViz {
  constructor(el, scene, boards, opts = {}) {
    this.data = scene; this.el = el; this.opts = opts;
    const base = boards[scene.base];
    this.viz = new BoardViz(el, base, { azimuth: scene.view?.azimuth ?? -1.0, elevation: scene.view?.elevation ?? 0.8, showLabels: false, onSelect: opts.onSelect });
    const v = this.viz; this.frame = new T.Group(); this.frame.rotation.x = -Math.PI / 2; this.frame.position.set(-v.W / 2, 0, v.H / 2); v.scene.add(this.frame);
    this.boards = { [scene.base]: { root: v.board, byId: v.byId, parts: v.parts, W: v.W, H: v.H, thick: 1.6, at: [0, 0, 0] } };
    for (const b of scene.boards || []) {
      const built = buildBoard(boards[b.id]); const g = new T.Group(); g.add(built.root);
      built.root.rotation.set(0, 0, 0); built.root.position.set(0, 0, 0);
      g.rotation.z = (b.rot || 0) * Math.PI / 180;
      const at = this.mate(b, built); g.position.set(at[0], at[1], at[2]); g.visible = false;
      this.frame.add(g); this.boards[b.id] = { ...built, group: g, at, home: at.slice() }; v.pickRoots.push(g);
    }
    this.things = {};
    for (const t of scene.things || []) {
      const g = (HOBBY[t.type] || PARTS[t.type])(t); g.position.set(t.at[0], t.at[1], t.at[2] || 0); g.rotation.z = (t.rot || 0) * Math.PI / 180;
      g.userData.part = t; g.traverse(o => { o.userData.partGroup = g; o.castShadow = true; });
      if (t.hidden) g.visible = false;
      this.frame.add(g); this.things[t.id] = g; v.pickRoots.push(g);
    }
    this.wires = {};
    for (const w of scene.wires || []) this.wires[w.id] = this.makeWire(w);
    v.labelParts = [...Object.values(this.boards).flatMap(b => b.parts || []), ...Object.values(this.things)];
    v.extent = scene.extent || null; v.resize();
    this.tweens = []; this.spinning = new Set(); this.flowing = new Set(); this.rotorAngle = {}; this.step = -1; this.playing = false; this.last = performance.now();
    this.rate = 1;              // playback speed: every tween, stagger and hold is divided by it (the speed menu)
    this.stepStart = 0; this.stepLen = 0; this.stepRunning = -1;   // the step being animated and its nominal length, for the scrub bar's fill
    v.onFrame = now => this.frameTick(now);
    this.focusPill = null;
    // the pins a wire has been drawn to, named small and always on (the board table's own pin ids, as the tags write them: "J2 20 GND")
    v.pinLabelFn = () => {
      const out = [];
      for (const m of Object.values(this.wires)) {
        if (!m.visible || !m.userData.wire) continue; const w = m.userData.wire, ends = w.ends || [];
        m.userData.ends.forEach((pt, i) => { const mm = /^J(\d+) pin (\d+) \((\w+)\)/.exec(ends[i] || ""); if (mm) out.push({ at: this.frame.localToWorld(pt.clone()), text: `J${mm[1]} ${mm[2]} ${mm[3]}` }); });
      }
      return out;
    };
  }

  // place a board so its named sockets sit on the base board's named headers: mate: [["j9","j1"],["j8","j2"]], lift: mm above the base
  mate(b, built) {
    if (b.at) return b.at;
    const base = this.boards[this.data.base]; const rot = (b.rot || 0) * Math.PI / 180; const c = Math.cos(rot), s = Math.sin(rot);
    let sx = 0, sy = 0, n = 0;
    for (const [mine, theirs] of b.mate) {
      const a = built.byId[mine], t = base.byId[theirs]; if (!a || !t) continue;
      const rx = a.position.x * c - a.position.y * s, ry = a.position.x * s + a.position.y * c;
      sx += t.position.x - rx; sy += t.position.y - ry; n++;
    }
    return [n ? sx / n : 0, n ? sy / n : 0, b.lift ?? 9];
  }
  // a place in the frame: "board.part" (the part's position, on top), "board.part.way2" (a terminal way), "thing.wireOut"
  where(ref) {
    const [owner, part, sub] = ref.split("."); const v = new T.Vector3();
    const b = this.boards[owner]; let obj;
    if (b) { obj = b.byId[part]; if (!obj) throw new Error("no part " + ref); if (sub && sub.startsWith("way")) obj = obj.userData.ways[+sub.slice(3)]; if (sub && sub.startsWith("pin")) obj = obj.children[+sub.slice(3)]; }
    else { obj = this.things[owner]; if (!obj) throw new Error("no thing " + ref); if (obj.userData.place) obj = obj.userData.place(part); else if (sub) obj = obj.userData[sub] || obj; }
    obj.getWorldPosition(v); return this.frame.worldToLocal(v);
  }
  makeWire(w) {
    const pts = [this.where(w.from), ...(w.via || []).map(p => new T.Vector3(...p)), this.where(w.to)];
    // leave a terminal straight out of its hole, then sag toward the far end
    // a wire never passes through a board: on a board that has been turned over, the pin points down, so the wire leaves it
    // downward and goes round the board's edge before it heads for the far end
    const flipped = ref => { const b = this.boards[ref.split(".")[0]]; return b && b.group && Math.abs(b.group.rotation.x) > 1 ? b : null; };
    const around = (p, b) => { const c = b.group.position; const d = new T.Vector3(p.x - c.x, p.y - c.y, 0); if (d.lengthSq() < 1e-6) d.set(1, 0, 0); d.normalize(); return new T.Vector3(p.x + d.x * 14, p.y + d.y * 14, p.z - 4); };
    const a = pts[0].clone(), z = pts[pts.length - 1].clone(); const fa = flipped(w.from), fz = flipped(w.to);
    if (w.exit) { const e = new T.Vector3(...w.exit); if (fa) e.z = -e.z; a.add(e); pts.splice(1, 0, a); }
    if (fa) pts.splice(w.exit ? 2 : 1, 0, around(a, fa));
    if (w.enter) { const e = new T.Vector3(...w.enter); if (fz) e.z = -e.z; z.add(e); pts.splice(pts.length - 1, 0, z); }
    if (fz) pts.splice(pts.length - (w.enter ? 2 : 1), 0, around(z, fz));
    const curve = new T.CatmullRomCurve3(pts, false, "centripetal", 0.6);
    const geo = new T.TubeGeometry(curve, Math.max(24, Math.round(curve.getLength() / 2)), w.d || 0.7, 8, false);
    const m = new T.Mesh(geo, mat(w.color || "#e0e0e0", { side: T.DoubleSide })); m.castShadow = true; m.visible = false; m.userData.wire = w;
    const total = geo.index.count; m.geometry.setDrawRange(0, 0); m.userData.total = total; m.userData.curve = curve; m.userData.ends = [pts[0].clone(), pts[pts.length - 1].clone()]; this.frame.add(m);
    return m;
  }
  // travelling dots along a wire: power, signal or drive, from the wire's from end unless dir is -1
  flowDots(id, kind, dir) {
    const m = this.wires[id]; if (m.userData.dots) m.userData.dots.visible = true;
    else {
      const g = new T.Group(); const n = Math.max(3, Math.round(m.userData.curve.getLength() / 10)); const col = kind === "power" ? "#ffcf3d" : kind === "drive" ? "#ff7a3d" : "#7fd8ff";
      for (let i = 0; i < n; i++) { const d = new T.Mesh(new T.SphereGeometry((m.userData.wire.d || 0.7) * 1.25, 10, 8), new T.MeshBasicMaterial({ color: col })); d.layers.set(1); g.add(d); }
      g.userData = { n, speed: kind === "power" ? 0.35 : kind === "drive" ? 0.5 : 0.25, dir: dir || 1, phase: 0 }; this.frame.add(g); m.userData.dots = g;
      g.children.forEach((d, i) => d.position.copy(m.userData.curve.getPointAt(i / n)));
    }
    this.flowing.add(id);
  }
  tween(dur, fn, done) { if (dur <= 1) { fn(1); this.viz.dirty = true; done && done(); return; } this.tweens.push({ t0: performance.now(), dur: dur / this.rate, fn, done }); fn(0); }
  // how long a step's actions take at 1x, from the same factors act() uses: the bar fills over this while the step plays
  stepLength(i) {
    let ms = 0;
    for (const a of this.data.steps[i].do || []) {
      const d = a.dur || 900;
      if (a.drop) ms += d * 1.4; else if (a.flip) ms += d * 1.6; else if (a.show) ms += d * 0.6; else if (a.switch) ms += d * 0.5;
      else if (a.wire) ms += d + ([].concat(a.wire).length - 1) * 250; else if (a.hide || a.screen || a.unwire || a.led || a.spin || a.stop || a.flow || a.noflow) ms += 0; else ms += d;
    }
    return Math.max(ms, 1);
  }
  progress() {   // [step, fraction through it], for the scrub bar
    if (this.stepRunning < 0) return [this.step, 1];
    return [this.stepRunning, Math.min(1, (performance.now() - this.stepStart) / (this.stepLen / this.rate))];
  }
  frameTick(now) {
    let changed = false; const dt = Math.min(0.05, (now - this.last) / 1000); this.last = now;
    for (const tw of this.tweens.slice()) {
      const t = Math.min(1, (now - tw.t0) / tw.dur); tw.fn(ease(t)); changed = true;
      if (t >= 1) { this.tweens.splice(this.tweens.indexOf(tw), 1); tw.done && tw.done(); }
    }
    if (this.stepRunning >= 0 && this.opts.onProgress) this.opts.onProgress(...this.progress());
    for (const id of this.spinning) {
      const g = this.things[id]; const r = g.userData.rotor; if (!r) continue;
      this.rotorAngle[id] = (this.rotorAngle[id] || 0) + dt * (g.userData.part.rpm || 300) / 60 * Math.PI * 2; if (g.userData.rotorAxis === "z") r.rotation.z = this.rotorAngle[id]; else r.rotation.x = this.rotorAngle[id]; changed = true;
    }
    for (const id of this.flowing) {
      const m = this.wires[id]; const g = m.userData.dots; if (!g || !m.visible) continue; const u = g.userData; u.phase = (u.phase + dt * u.speed) % 1;
      g.children.forEach((d, i) => { let t = (i / u.n + u.phase) % 1; if (u.dir < 0) t = 1 - t; d.position.copy(m.userData.curve.getPointAt(t)); }); changed = true;
    }
    return changed;
  }
  // tags at both ends of the step's wires: the board end always, the far end once per shared point (a motor lead)
  showTags(ids) {
    const seen = new Map(); const tags = [];
    for (const id of ids) {
      const m = this.wires[id]; const w = m.userData.wire; const ends = w.ends || []; const [a, z] = m.userData.ends.map(v => this.frame.localToWorld(v.clone()));
      if (ends[1]) tags.push({ at: z, text: ends[1], color: w.color });
      const k = a.toArray().map(x => Math.round(x / 4)).join(","); if (ends[0] && !seen.has(k)) { seen.set(k, 1); tags.push({ at: a, text: ends[0], color: w.color, cls: "bviz-far" }); }
    }
    this.viz.extraLabels = tags; this.viz.dirty = true;
  }
  // ----- actions -----
  act(a, dur = 900) {
    return new Promise(res => {
      if (a.drop) { const b = this.boards[a.drop]; b.group.visible = true; const [x, y, z] = b.home; this.tween(dur * 1.4, t => b.group.position.set(x, y, z + (1 - t) * 45), res); }
      else if (a.lift) { const b = this.boards[a.lift]; const [x, y, z] = b.home; this.tween(dur, t => b.group.position.set(x, y, z + t * 45), () => { b.group.visible = false; res(); }); }
      else if (a.show) { const g = this.things[a.show] || this.boards[a.show]?.group; g.visible = true; const s = g.scale.clone(); this.tween(dur * 0.6, t => g.scale.set(0.001 + t, 0.001 + t, 0.001 + t), () => { g.scale.copy(s); res(); }); }
      else if (a.hide) { const g = this.things[a.hide] || this.boards[a.hide]?.group; g.visible = false; res(); }
      else if (a.flip) { // turn a board over about its own long axis, lifting it as it turns; wires that end on it are re-routed to the pins' new places
        const b = this.boards[a.flip]; const g = b.group; const [x, y, z] = b.home; const from = g.rotation.x, to = a.back ? 0 : Math.PI;
        this.tween(dur * 1.6, t => { g.rotation.x = from + (to - from) * t; g.position.set(x, y, z + Math.sin(t * Math.PI) * 30); }, () => { g.rotation.x = to; g.position.set(x, y, z); this.rewire(a.flip); res(); }); }
      else if (a.screen) { // a picture on a part's face: the part's group gets a textured plane (w by h mm, local z at the part's height), a screen lit up
        const g = this.part(a.screen); if (!g) return res();
        let pl = g.userData.screenPlane;
        if (!pl) { const tex = new T.TextureLoader().load(a.image, () => { this.viz.dirty = true; }); tex.colorSpace = T.SRGBColorSpace; tex.anisotropy = 4;
          pl = new T.Mesh(new T.PlaneGeometry(a.w || 49, a.h || 73.4), new T.MeshBasicMaterial({ map: tex, toneMapped: false }));
          pl.position.set(a.dx || 0, a.dy || 0, (g.userData.part?.d ?? 3) + 0.06); pl.rotation.z = (a.rot || 0) * Math.PI / 180; if (a.mirror) pl.scale.x = -1; pl.userData.silk = true; g.add(pl); g.userData.screenPlane = pl; }
        pl.visible = !a.off; this.viz.dirty = true; res(); }
      else if (a.wire) { const ids = [].concat(a.wire); let left = ids.length; ids.forEach((id, i) => { const m = this.wires[id]; m.visible = true; const go = () => this.tween(dur, t => m.geometry.setDrawRange(0, Math.round(m.userData.total * t)), () => { if (--left === 0) { this.showTags(ids); res(); } }); if (dur > 1) setTimeout(go, i * 250 / this.rate); else go(); }); }
      else if (a.unwire) { for (const id of [].concat(a.unwire)) { const m = this.wires[id]; m.visible = false; m.geometry.setDrawRange(0, 0); } res(); }
      else if (a.switch) { const g = this.part(a.switch); const k = g.userData.knob, th = g.userData.throw; const from = k.position.y, to = (a.pos === "down" ? -1 : 1) * th / 2; this.tween(dur * 0.5, t => { k.position.y = from + (to - from) * t; }, res); }
      else if (a.shunt) { const g = this.part(a.shunt); const sh = g.children.filter(c => c.isGroup); const target = sh[0]; if (!target) return res(); const z0 = target.position.z; this.tween(dur, t => { target.position.z = z0 + (1 - t) * 12; }, res); }
      else if (a.led) { for (const ref of [].concat(a.led)) { const g = this.part(ref); g.traverse(o => { if (o.isMesh && o.material.emissive !== undefined && o.material.emissiveIntensity !== undefined) { o.material = o.material.clone(); o.material.emissiveIntensity = a.off ? 0 : 1.2; } }); } res(); }
      else if (a.flow) { for (const id of [].concat(a.flow)) this.flowDots(id, a.kind || "signal", a.dir); res(); }
      else if (a.noflow) { for (const id of [].concat(a.noflow)) { this.flowing.delete(id); const g = this.wires[id].userData.dots; if (g) g.visible = false; } res(); }
      else if (a.tags) { this.showTags([].concat(a.tags)); res(); }
      else if (a.spin) { this.spinning.add(a.spin); res(); }
      else if (a.stop) { this.spinning.delete(a.stop); res(); }
      else if (a.view && this.fixedView) { res(); /* the home page's scroll build keeps one camera (maintainer, 2026-11-09) */ }
      else if (a.view) { const v = this.viz.view; const from = { a: v.azimuth, e: v.elevation, z: v.zoom, t: v.target.clone() }; const to = { a: a.view.azimuth ?? from.a, e: a.view.elevation ?? from.e, z: a.view.zoom ?? from.z, t: typeof a.view.center === "string" ? this.frame.localToWorld(this.where(a.view.center)) : a.view.center ? new T.Vector3(a.view.center[0] - this.viz.W / 2, a.view.center[2] || 0, this.viz.H / 2 - a.view.center[1]) : new T.Vector3(...(a.view.target || [from.t.x, from.t.y, from.t.z])) }; this.tween(dur, t => { v.azimuth = from.a + (to.a - from.a) * t; v.elevation = from.e + (to.e - from.e) * t; v.zoom = from.z + (to.z - from.z) * t; v.target.lerpVectors(from.t, to.t, t); }, res); }
      else if (a.focus) { this.focus = a.focus; this.viz.dirty = true; res(); }
      else res();
    });
  }
  part(ref) { const [owner, part] = ref.split("."); const b = this.boards[owner]; return b ? b.byId[part] : this.things[owner]; }
  rewire(board) {
    for (const [id, old] of Object.entries(this.wires)) {
      const w = old.userData.wire; if (w.from.split(".")[0] !== board && w.to.split(".")[0] !== board) continue;
      const shown = old.visible, dots = old.userData.dots; this.frame.remove(old); old.geometry.dispose();
      const m = this.makeWire(w); m.visible = shown; if (shown) m.geometry.setDrawRange(0, m.userData.total); if (dots) { m.userData.dots = dots; dots.visible = false; }
      this.wires[id] = m;
    }
    this.viz.dirty = true;
  }
  // ----- steps -----
  reset() {
    this.tweens = []; this.spinning.clear();
    for (const [id, b] of Object.entries(this.boards)) if (id !== this.data.base) { b.group.visible = false; b.group.position.set(...b.home); if (b.group.rotation.x) { b.group.rotation.x = 0; this.rewire(id); } }
    for (const b of Object.values(this.boards)) for (const g of Object.values(b.byId || {})) if (g.userData.screenPlane) g.userData.screenPlane.visible = false;
    for (const [id, g] of Object.entries(this.things)) { const t = g.userData.part; g.visible = !t.hidden; if (g.userData.rotor) g.userData.rotor.rotation.set(0, 0, 0); }
    for (const m of Object.values(this.wires)) { m.visible = false; m.geometry.setDrawRange(0, 0); if (m.userData.dots) m.userData.dots.visible = false; }
    this.flowing.clear(); this.viz.extraLabels = [];
    for (const s of this.data.steps) for (const a of s.do || []) { if (a.led) for (const ref of [].concat(a.led)) this.part(ref).traverse(o => { if (o.isMesh && o.material.emissiveIntensity > 0.6) o.material.emissiveIntensity = 0.55; }); if (a.switch) { const g = this.part(a.switch); g.userData.knob.position.y = g.userData.throw / 2; } }
    this.focus = null; this.viz.focusObj = null; this.viz.dirty = true;
  }
  async goto(n, animate = true) {
    const steps = this.data.steps; n = Math.max(0, Math.min(steps.length - 1, n));
    if (n <= this.step || this.step < 0 || n > this.step + 1) { this.reset(); this.step = -1; for (let i = 0; i < n; i++) await this.run(i, false); }
    await this.run(n, animate); this.step = n; this.opts.onStep && this.opts.onStep(n, steps[n]);
  }
  async run(i, animate) {
    const s = this.data.steps[i]; this.focus = s.focus || null; this.viz.extraLabels = []; this.stepView = (s.do || []).find(a => a.view) || this.stepView;
    const fo = s.focus ? this.part(s.focus) : null; this.viz.focusObj = fo; this.viz.focusLabel = s.focus_label || (fo && fo.userData.part && fo.userData.part.label) || "";
    if (animate) { this.stepRunning = i; this.stepStart = performance.now(); this.stepLen = this.stepLength(i); }
    for (const a of s.do || []) await this.act(a, animate ? (a.dur || 900) : 1);
    if (!animate) { for (const tw of this.tweens) { tw.fn(1); tw.done && tw.done(); } this.tweens = []; }
    this.stepRunning = -1; this.opts.onProgress && this.opts.onProgress(i, 1);
  }
  async play() {
    this.playing = true;
    for (let i = this.step + 1; i < this.data.steps.length && this.playing; i++) { await this.goto(i); await new Promise(r => setTimeout(r, (this.data.steps[i].hold || 1600) / this.rate)); }
    this.playing = false; this.opts.onPlayEnd && this.opts.onPlayEnd();
  }
  stop() { this.playing = false; }
}

// Mount every <div class="bviz-scene" data-scene=...> with the scene JSON in #bviz-scene-<id> and each board's JSON in #bviz-<board>.
export function mountScenes() {
  // a scene in a version the page is not showing (display none) mounts when it is first shown: two live WebGL contexts at load lost one
  const pending = [];
  const mountOne = (wrap) => {
    const id = wrap.dataset.scene; const src = document.getElementById("bviz-scene-" + id); if (!src) return;
    const scene = JSON.parse(src.textContent); const boards = {};
    for (const b of [scene.base, ...(scene.boards || []).map(x => x.id)]) { const e = document.getElementById("bviz-" + b); if (e) boards[b] = JSON.parse(e.textContent); }
    const el = wrap.querySelector(".bviz"); const list = wrap.querySelector(".bviz-steps"); const card = wrap.querySelector(".bviz-card");
    const playBtn = wrap.querySelector("[data-bviz=play]");
    // the scrub bar: one segment per step, a fill that follows the play head, drag or tap to jump (fast-forward to the spot and stop)
    const bar = wrap.querySelector(".bviz-bar"); const N = scene.steps.length; let fill = null;
    if (bar) {
      bar.textContent = ""; bar.setAttribute("role", "slider"); bar.setAttribute("aria-label", "Build progress"); bar.setAttribute("aria-valuemin", "1"); bar.setAttribute("aria-valuemax", String(N)); bar.tabIndex = 0;
      fill = document.createElement("i"); fill.className = "bviz-bar-fill"; bar.appendChild(fill);
      for (let i = 1; i < N; i++) { const t = document.createElement("b"); t.className = "bviz-bar-tick"; t.style.left = (i / N * 100) + "%"; bar.appendChild(t); }
      const knob = document.createElement("s"); knob.className = "bviz-bar-knob"; bar.appendChild(knob);
      for (let i = 0; i < N; i++) { const h = document.createElement("u"); h.className = "bviz-bar-seg"; h.style.left = (i / N * 100) + "%"; h.style.width = (100 / N) + "%"; h.title = `${i + 1}. ${scene.steps[i].title}`; bar.appendChild(h); }
    }
    const setBar = (n, f) => { if (!bar) return; const x = Math.max(0, Math.min(1, (n + f) / N)); fill.style.width = (x * 100) + "%"; bar.querySelector(".bviz-bar-knob").style.left = (x * 100) + "%"; bar.setAttribute("aria-valuenow", String(n + 1)); bar.setAttribute("aria-valuetext", `${n + 1} of ${N}: ${scene.steps[n] ? scene.steps[n].title : ""}`); };
    let sv;
    try { sv = new SceneViz(el, scene, boards, {
      onStep: (n, s) => { list && list.querySelectorAll("li").forEach((li, i) => li.classList.toggle("on", i === n)); if (card) { card.hidden = false; card.textContent = ""; const b = document.createElement("b"); b.textContent = `${n + 1}. ${s.title}`; card.appendChild(b); const p = document.createElement("p"); p.textContent = s.text || ""; card.appendChild(p); if (s.check) { const c = document.createElement("em"); c.className = "bviz-check"; c.textContent = s.check; card.appendChild(c); } } },
      onPlayEnd: () => { if (playBtn) playBtn.textContent = "Play again"; },
      onProgress: (n, f) => setBar(n, f),
      onSelect: d => { if (!d || !card) return; card.hidden = false; card.textContent = ""; const b = document.createElement("b"); b.textContent = d.title; card.appendChild(b); if (d.text) { const p = document.createElement("p"); p.textContent = d.text; card.appendChild(p); } },
    }); } catch (e) { console.error("boardviz scene", e); el.classList.add("bviz-nogl"); return; }
    if (list) list.querySelectorAll("li").forEach((li, i) => li.addEventListener("click", () => { sv.stop(); sv.goto(i); }));
    wrap.querySelectorAll("[data-bviz]").forEach(b => b.addEventListener("click", () => {
      const a = b.dataset.bviz;
      if (a === "play") { if (sv.playing) { sv.stop(); b.textContent = "Play"; } else { if (sv.step >= scene.steps.length - 1) { sv.reset(); sv.step = -1; } b.textContent = "Pause"; sv.play(); } }
      if (a === "next") { sv.stop(); sv.goto(sv.step + 1); } if (a === "prev") { sv.stop(); sv.goto(sv.step - 1); }
      if (a === "reset") { if (sv.stepView) sv.act(sv.stepView, 500); else sv.viz.resetView(); } if (a === "top") sv.viz.topView();
    }));
    const speed = wrap.querySelector("[data-bviz-speed]");
    if (speed) speed.addEventListener("change", () => { sv.rate = +speed.value || 1; });
    if (bar) {   // drag or tap: jump to that step at once, playback stops; release where you want to look
      const pick = ev => { const r = bar.getBoundingClientRect(); return Math.max(0, Math.min(N - 1, Math.floor((ev.clientX - r.left) / r.width * N))); };
      let dragging = false, lastN = -1;
      const jump = n => { if (n === lastN) return; lastN = n; sv.stop(); if (playBtn) playBtn.textContent = "Play"; setBar(n, 1); sv.goto(n, false); };
      bar.addEventListener("pointerdown", ev => { dragging = true; bar.setPointerCapture(ev.pointerId); jump(pick(ev)); });
      bar.addEventListener("pointermove", ev => { if (dragging) jump(pick(ev)); });
      bar.addEventListener("pointerup", () => { dragging = false; lastN = -1; }); bar.addEventListener("pointercancel", () => { dragging = false; lastN = -1; });
      bar.addEventListener("keydown", ev => {   // arrows step, space plays or pauses, Home and End jump to the ends
        const k = ev.key; if (!["ArrowLeft", "ArrowRight", "Home", "End", " "].includes(k)) return; ev.preventDefault();
        if (k === " ") { playBtn && playBtn.click(); return; }
        sv.stop(); if (playBtn) playBtn.textContent = "Play";
        sv.goto(k === "ArrowLeft" ? sv.step - 1 : k === "ArrowRight" ? sv.step + 1 : k === "Home" ? 0 : N - 1, false);
      });
    }
    const onStep0 = sv.opts.onStep; sv.opts.onStep = (n, st) => { setBar(n, 1); onStep0 && onStep0(n, st); };
    wrap.sceneViz = sv; wrap.ready = sv.goto(+(wrap.dataset.step || 0), false);
  };
  const sync = () => { for (let i = pending.length - 1; i >= 0; i--) if (pending[i].offsetParent !== null) mountOne(pending.splice(i, 1)[0]); };
  for (const wrap of document.querySelectorAll(".bviz-scene[data-scene]")) (wrap.offsetParent === null ? pending.push(wrap) : mountOne(wrap));
  if (pending.length) window.addEventListener("frl-route", () => requestAnimationFrame(sync));
}
