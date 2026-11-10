// Drawn 3D board for board pages. Data: data/boardviz/<board>.json (its _about says the shape).
// Built from primitives in code, no model files. Default style "real": metal and plastic materials lit by a
// small studio environment, antialiased, no ink lines. Style "drawn" (?bviz=drawn) keeps the toon shading and
// depth-and-normal outline pass. The scene renders only when something changes, and shadows are frozen after
// the first frame, so it stays smooth on a phone. See tools/boardviz/README.md.
import * as T from "../vendor/three-0.182.0.module.min.js";

const PITCH = 2.54;
const COL = {
  pad: "#d8b45c", pin: "#cfc9bd", pinDark: "#5a5650", chip: "#2e2f33", chipLight: "#45464b", header: "#1c1c1f",
  shunt: "#232326", shuntDark: "#141416", silver: "#c6c4bf", silverDark: "#8d8b86", button: "#8e8e92", buttonCap: "#3a3a3d",
  ring: "#d9b55e", hole: "#2a1a16", silk: "#f4efe6", cream: "#e9dcc3", resBody: "#2a2a2e", capBody: "#b58a52", soic: "#303134",
  ground: "#7a5532", line: "#141a2c",
};

// ---------- materials ----------
const STYLE = (typeof location !== "undefined" && new URLSearchParams(location.search).get("bviz") === "drawn") ? "drawn" : "real";
// surface finish per palette color in the real style: plated metals reflect, epoxy and plastics are satin
const FINISH = new Map([
  [COL.pad, { metalness: 1, roughness: 0.26 }], [COL.ring, { metalness: 1, roughness: 0.26 }],
  [COL.pin, { metalness: 1, roughness: 0.3 }], [COL.pinDark, { metalness: 0.7, roughness: 0.5 }],
  [COL.silver, { metalness: 1, roughness: 0.32 }], [COL.silverDark, { metalness: 1, roughness: 0.45 }],
  [COL.button, { metalness: 0.9, roughness: 0.38 }],
  [COL.chip, { roughness: 0.62 }], [COL.chipLight, { roughness: 0.6 }], [COL.soic, { roughness: 0.62 }],
  [COL.header, { roughness: 0.42 }], [COL.buttonCap, { roughness: 0.5 }], [COL.hole, { roughness: 0.95 }],
  [COL.shunt, { roughness: 0.38 }], [COL.shuntDark, { roughness: 0.45 }], [COL.cream, { roughness: 0.4 }],
]);
let gradientTex = null;
function gradient() {
  if (gradientTex) return gradientTex;
  const c = document.createElement("canvas"); c.width = 4; c.height = 1;
  const g = c.getContext("2d");
  [[0, "#9c9c9c"], [1, "#d6d6d6"], [2, "#ffffff"], [3, "#ffffff"]].forEach(([i, col]) => { g.fillStyle = col; g.fillRect(i, 0, 1, 1); });
  gradientTex = new T.CanvasTexture(c); gradientTex.minFilter = T.NearestFilter; gradientTex.magFilter = T.NearestFilter; gradientTex.colorSpace = T.NoColorSpace;
  return gradientTex;
}
const matCache = new Map();
function mat(color, opts = {}) {
  const key = color + JSON.stringify(opts);
  if (matCache.has(key)) return matCache.get(key);
  let m;
  if (STYLE === "drawn") { const { clearcoat, clearcoatRoughness, metalness, roughness, ...rest } = opts; m = new T.MeshToonMaterial({ color, gradientMap: gradient(), ...rest }); }
  else { const M = opts.clearcoat !== undefined ? T.MeshPhysicalMaterial : T.MeshStandardMaterial; m = new M({ color, metalness: 0, roughness: 0.5, ...(FINISH.get(color) || {}), ...opts }); }
  matCache.set(key, m);
  return m;
}
function emissive(color) {
  if (STYLE === "drawn") return new T.MeshToonMaterial({ color, emissive: color, emissiveIntensity: 0.55, gradientMap: gradient() });
  return new T.MeshStandardMaterial({ color, emissive: color, emissiveIntensity: 1.4, roughness: 0.35 });
}
// a soft studio for reflections: a dim room with one big overhead softbox and two side strips (the idea of three.js RoomEnvironment)
function studio(renderer) {
  const s = new T.Scene(); const basic = v => new T.MeshBasicMaterial({ color: new T.Color(v, v, v), side: T.DoubleSide });
  const room = new T.Mesh(new T.BoxGeometry(20, 12, 20), new T.MeshBasicMaterial({ color: new T.Color(0.55, 0.54, 0.52), side: T.BackSide })); room.position.y = 4; s.add(room);
  const panel = (w, h, v, pos, rot) => { const m = new T.Mesh(new T.PlaneGeometry(w, h), basic(v)); m.position.set(...pos); m.rotation.set(...rot); s.add(m); };
  panel(9, 9, 5.5, [0, 9.9, 0], [Math.PI / 2, 0, 0]);
  panel(2, 8, 2.6, [-9.9, 4, 0], [0, Math.PI / 2, 0]); panel(2, 8, 1.8, [9.9, 4, 2], [0, -Math.PI / 2, 0]);
  panel(8, 3, 1.2, [0, 3, 9.9], [0, Math.PI, 0]);
  const pm = new T.PMREMGenerator(renderer); const tex = pm.fromScene(s, 0.04).texture; pm.dispose(); return tex;
}

// ---------- geometry helpers (x right, y up on the board, z toward the viewer; the whole board is rotated flat later) ----------
function box(w, h, d, m, x = 0, y = 0, z = 0) {
  const g = new T.Mesh(new T.BoxGeometry(w, h, d), m);
  g.position.set(x, y, z + d / 2);
  return g;
}
function cyl(r, hgt, m, x = 0, y = 0, z = 0, seg = 18) {
  const g = new T.Mesh(new T.CylinderGeometry(r, r, hgt, seg), m);
  g.rotation.x = Math.PI / 2; g.position.set(x, y, z + hgt / 2);
  return g;
}
function roundedRect(w, h, r) {
  const s = new T.Shape(); const x = -w / 2, y = -h / 2;
  s.moveTo(x + r, y); s.lineTo(x + w - r, y); s.quadraticCurveTo(x + w, y, x + w, y + r);
  s.lineTo(x + w, y + h - r); s.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
  s.lineTo(x + r, y + h); s.quadraticCurveTo(x, y + h, x, y + h - r);
  s.lineTo(x, y + r); s.quadraticCurveTo(x, y, x + r, y);
  return s;
}
function textPlane(text, size, color, rot = 0, font = "600 48px Barlow, 'Barlow Semi Condensed', sans-serif") {
  const c = document.createElement("canvas"); const g = c.getContext("2d");
  g.font = font; const w = Math.ceil(g.measureText(text).width) + 16; c.width = w; c.height = 64;
  g.font = font; g.fillStyle = color; g.textBaseline = "middle"; g.fillText(text, 8, 32);
  const tex = new T.CanvasTexture(c); tex.colorSpace = T.SRGBColorSpace; tex.anisotropy = 4;
  const m = new T.MeshBasicMaterial({ map: tex, transparent: true, depthWrite: false });
  const mesh = new T.Mesh(new T.PlaneGeometry(size * w / 48, size * 64 / 48), m);
  mesh.rotation.z = rot * Math.PI / 180; mesh.userData.silk = true;
  return mesh;
}

// ---------- part library ----------
const PARTS = {
  block(p) { const g = new T.Group(); g.add(box(p.w || 20, p.h || 12, p.d || 10, mat(p.color || "#444"))); if (p.text2) { const t = textPlane(p.text2, p.ts || 3, COL.silk); t.position.set(0, 0, (p.d || 10) + 0.05); g.add(t); } return g; },
  terminal(p) { // a blue screw terminal block, n ways at 5 mm, screws on top, wire holes on the +y face (away from the board center); names per way
    const g = new T.Group(); const n = p.ways || 2, pitch = p.pitch || 5.0, W = n * pitch, H = p.h || 8, D = p.d || 10;
    g.add(box(W, H, D, mat(p.color || "#2a62c8")));
    for (let i = 0; i < n; i++) { const x = -W / 2 + pitch / 2 + i * pitch; g.add(cyl(pitch * 0.3, 0.5, mat(COL.silver), x, -H * 0.2, D, 14)); const hole = box(pitch * 0.5, 0.3, D * 0.4, mat(COL.hole), x, H / 2 - 0.15, D * 0.25); g.add(hole); const a = new T.Group(); a.position.set(x, H / 2, D * 0.45); a.userData.wayName = p.names && p.names[i]; g.add(a); g.userData.ways = g.userData.ways || []; g.userData.ways.push(a); }
    return g;
  },
  hallblock(p) { // the green 5-way Hall plug: wire holes on the -x face
    const g = new T.Group(); const n = p.ways || 5, pitch = p.pitch || 3.5, H = n * pitch, W = p.w || 8, D = p.d || 9;
    g.add(box(W, H, D, mat(p.color || "#3fae5c")));
    for (let i = 0; i < n; i++) { const y = -H / 2 + pitch / 2 + i * pitch; g.add(box(0.3, pitch * 0.5, D * 0.4, mat(COL.hole), -W / 2 + 0.15, y, D * 0.25)); g.add(cyl(pitch * 0.22, 0.4, mat(COL.silver), 0, y, D, 10)); const a = new T.Group(); a.position.set(-W / 2, y, D * 0.45); a.userData.wayName = p.names && p.names[i]; g.add(a); g.userData.ways = g.userData.ways || []; g.userData.ways.push(a); }
    return g;
  },
  bigcap(p) { const g = new T.Group(); const r = (p.d || 10) / 2, h = p.h || 12; g.add(cyl(r, h, mat(p.color || "#1f3b8a"), 0, 0, 0, 28)); g.add(cyl(r * 0.92, 0.2, mat(COL.silver), 0, 0, h, 28)); g.add(box(r * 0.5, h, 0.05, mat(COL.silver), -r * 0.6, 0, h / 2)); return g; },
  slideswitch(p) { const g = new T.Group(); g.add(box(p.w || 3.5, p.h || 7, 2.0, mat(COL.silver))); const k = box(1.4, 1.6, 1.2, mat(COL.chip), 0, (p.h || 7) * 0.22 * (p.pos === "down" ? -1 : 1), 2.0); k.userData.knob = true; g.userData.knob = k; g.userData.throw = (p.h || 7) * 0.44; g.add(k); return g; },
  usbc() { const g = new T.Group(); g.add(box(9.0, 7.5, 3.2, mat(COL.silver), 0, 0.6, 0)); g.add(box(7.4, 1.0, 2.0, mat(COL.chip), 0, 4.0, 0.6)); return g; },
  motor(p) { // a 42 mm square brushless motor standing on its back, shaft up: black anodized end cap and front flange,
    // a brushed aluminum can with chamfered corners, four socket-head screws, a pilot boss, the shaft with its D flat,
    // and the cable leaving the back through a grommet. The red keyed disc on the shaft shows it turning.
    const g = new T.Group(); const S = p.size || 42, L = p.len || 60, r = S / 2;
    const ext = (w, depth, round, bev) => new T.ExtrudeGeometry(roundedRect(w, w, round), { depth, bevelEnabled: true, bevelThickness: bev, bevelSize: bev, bevelSegments: 3 });
    const anod = mat("#1e1f22", { metalness: 0.55, roughness: 0.42 }), alu = mat(p.color || "#b8bbc0", { metalness: 1, roughness: 0.34 });
    const steel = mat("#d7d9dc", { metalness: 1, roughness: 0.18 }), rubber = mat("#151515", { roughness: 0.85 });
    const capB = new T.Mesh(ext(S - 1, 7, 6, 0.8), anod); capB.position.z = 0.8; g.add(capB);                                             // back end cap
    const can = new T.Mesh(ext(S - 3, L - 18, 4, 0.6), alu); can.position.z = 8.6; g.add(can);                                            // can
    const flange = new T.Mesh(ext(S - 1, 7, 6, 0.8), anod); flange.position.z = L - 8.2; g.add(flange);                                   // front flange
    for (const sx of [-1, 1]) for (const sy of [-1, 1]) {                                                                                 // socket-head screws
      const x = sx * (r - 5.5), y = sy * (r - 5.5);
      g.add(cyl(2.6, 1.6, steel, x, y, L - 0.5, 20)); g.add(cyl(1.25, 0.2, mat("#0c0c0d"), x, y, L + 1.05, 6));
    }
    g.add(cyl(11, 1.8, alu, 0, 0, L - 0.4, 40)); g.add(cyl(7, 0.6, anod, 0, 0, L + 1.4, 32));                                          // pilot boss and bearing seal
    const rotor = new T.Group(); rotor.position.set(0, 0, L + 2);
    rotor.add(cyl(2.5, 16, steel, 0, 0, 0, 24));                                                                                          // shaft
    rotor.add(box(1.0, 5.0, 12, mat("#9a9ca0", { metalness: 1, roughness: 0.3 }), 2.1, 0, 3));                                         // the D flat
    rotor.add(cyl(9, 2.5, mat("#B42418", { metalness: 0.4, roughness: 0.38 }), 0, 0, 9, 40)); rotor.add(box(16, 2.4, 0.4, mat(COL.silk), 0, 0, 11.5)); rotor.add(cyl(1.4, 0.4, mat(COL.silk), 5.5, 0, 11.5, 12));   // keyed disc
    g.add(rotor); g.userData.rotor = rotor; g.userData.rotorAxis = "z";
    const grom = cyl(3.2, 4, rubber, 0, 0, 0, 20); grom.rotation.set(0, 0, 0); grom.position.set(0, -r - 1.2, 6); g.add(grom);   // grommet on the back face, low
    const back = new T.Group(); back.position.set(0, -r - 4, 6); g.add(back); g.userData.wireOut = back;
    return g;
  },
  chip(p) { // a two-pad part: resistor or capacitor
    const g = new T.Group(); const L = p.len || 1.6, W = p.w || 0.8, cap = p.capacitor ?? (W < 0.75);
    g.add(box(L * 0.62, W * 0.95, 0.45, mat(cap ? COL.capBody : COL.resBody)));
    g.add(box(L * 0.2, W, 0.5, mat(COL.pin), -L * 0.4, 0, 0)); g.add(box(L * 0.2, W, 0.5, mat(COL.pin), L * 0.4, 0, 0));
    return g;
  },
  qfp(p) { // body with gull-wing pins on four sides
    const g = new T.Group(); const B = p.body || 10, n = p.pins || 16, span = B + 2;
    const body = new T.Mesh(new T.ExtrudeGeometry(roundedRect(B, B, 0.6), { depth: 1.4, bevelEnabled: true, bevelThickness: 0.25, bevelSize: 0.25, bevelSegments: 1 }), mat(COL.chip));
    body.position.z = 0.25; g.add(body);
    const dot = cyl(0.45, 0.05, mat(COL.chipLight), -B / 2 + 1.3, B / 2 - 1.3, 1.9, 12); g.add(dot);
    const pitch = (B - 1.6) / (n - 1), pinM = mat(COL.pin);
    for (let i = 0; i < n; i++) {
      const t = -(B - 1.6) / 2 + i * pitch;
      g.add(box(pitch * 0.45, (span - B) / 2 + 0.3, 0.2, pinM, t, B / 2 + (span - B) / 4 - 0.15, 0));
      g.add(box(pitch * 0.45, (span - B) / 2 + 0.3, 0.2, pinM, t, -B / 2 - (span - B) / 4 + 0.15, 0));
      g.add(box((span - B) / 2 + 0.3, pitch * 0.45, 0.2, pinM, B / 2 + (span - B) / 4 - 0.15, t, 0));
      g.add(box((span - B) / 2 + 0.3, pitch * 0.45, 0.2, pinM, -B / 2 - (span - B) / 4 + 0.15, t, 0));
    }
    return g;
  },
  qfn(p) { const s = p.size || 3; const g = new T.Group(); g.add(box(s, s, 0.8, mat(COL.chipLight))); g.add(cyl(0.2, 0.05, mat(COL.silk), -s / 2 + 0.5, s / 2 - 0.5, 0.8, 8)); return g; },
  soic(p) { const g = new T.Group(); const n = (p.pins || 8) / 2; g.add(box(4.9, 3.9, 1.5, mat(COL.soic))); for (let i = 0; i < n; i++) { const x = -1.9 + i * 1.27; g.add(box(0.45, 1.0, 0.2, mat(COL.pin), x, 2.45, 0)); g.add(box(0.45, 1.0, 0.2, mat(COL.pin), x, -2.45, 0)); } return g; },
  sot23(p) { const g = new T.Group(); const w = p.w || 1.4; g.add(box(2.9, w, 1.0, mat(COL.chip))); g.add(box(0.4, 0.6, 0.2, mat(COL.pin), -0.95, w / 2 + 0.3, 0)); g.add(box(0.4, 0.6, 0.2, mat(COL.pin), 0.95, w / 2 + 0.3, 0)); g.add(box(0.4, 0.6, 0.2, mat(COL.pin), 0, -w / 2 - 0.3, 0)); return g; },
  sot223() { const g = new T.Group(); g.add(box(3.6, 6.5, 1.6, mat(COL.chip))); g.add(box(2.6, 1.2, 0.3, mat(COL.pin), 0, 3.6, 0)); for (const y of [-2.3, 0, 2.3]) g.add(box(1.0, 0.7, 0.2, mat(COL.pin), -2.3, y, 0)); return g; },
  sma() { const g = new T.Group(); g.add(box(4.4, 2.6, 2.0, mat(COL.chip))); g.add(box(0.3, 2.6, 2.02, mat(COL.silver), -1.6, 0, 0)); g.add(box(1.0, 1.5, 0.25, mat(COL.pin), -2.6, 0, 0)); g.add(box(1.0, 1.5, 0.25, mat(COL.pin), 2.6, 0, 0)); return g; },
  crystal(p) { const g = new T.Group(); const w = p.w || 3.2, h = p.h || 1.5; const m = new T.Mesh(new T.CapsuleGeometry(h / 2, w - h, 4, 10), mat(COL.silver)); m.rotation.z = Math.PI / 2; m.position.z = h / 2; g.add(m); return g; },
  xtalbox(p) { const g = new T.Group(); g.add(box(p.w || 3.2, p.h || 2.5, 0.9, mat(COL.silverDark))); g.add(box((p.w || 3.2) - 0.3, (p.h || 2.5) - 0.3, 0.1, mat(COL.silver), 0, 0, 0.9)); return g; },
  led(p) { const g = new T.Group(); g.add(box(1.8, 1.0, 0.5, mat(COL.cream))); g.add(box(1.0, 0.8, 0.3, emissive(p.color || "#ff3020"), 0, 0, 0.5)); g.add(box(0.3, 1.0, 0.52, mat(COL.pin), -1.0, 0, 0)); g.add(box(0.3, 1.0, 0.52, mat(COL.pin), 1.0, 0, 0)); return g; },
  lcd(p) { // a small color screen module: white plastic frame, black glass, a picture plane, orange flex tail. g.userData.setImage(url, done) puts a picture on the glass; setImage(null) blanks it.
    const g = new T.Group(); const W = p.w || 33.8, H = p.h || 38.6, S = p.screen || 26, G = p.glass || 29.6;
    g.add(box(W, H, 2.2, mat("#e9e8e3", { roughness: 0.45 })));
    g.add(box(W - 8, H - 7.5, 0.5, mat("#d7d6d0", { roughness: 0.5 }), 0, 0.8, 2.2));
    const gy = 1.8; g.add(box(G, G, 1.6, mat("#0b0b0e", { roughness: 0.18, clearcoat: 1, clearcoatRoughness: 0.08 }), 0, gy, 2.2));
    g.add(box(G - 1.2, 0.3, 0.2, mat("#26262b"), 0, gy + G / 2 + 0.9, 3.0));
    g.add(box(20, 5.5, 0.25, mat("#b9842a", { roughness: 0.5 }), 0, -H / 2 + 4.2, 2.2));
    const cv = document.createElement("canvas"); cv.width = 128; cv.height = 128; const cx = cv.getContext("2d"); cx.fillStyle = "#050507"; cx.fillRect(0, 0, 128, 128);
    const tex = new T.CanvasTexture(cv); tex.colorSpace = T.SRGBColorSpace; tex.magFilter = T.NearestFilter; tex.anisotropy = 4;
    const scr = new T.Mesh(new T.PlaneGeometry(S, S), new T.MeshBasicMaterial({ map: tex, toneMapped: false })); scr.position.set(0, gy, 3.85); g.add(scr);
    g.userData.setImage = (src, done) => {
      if (!src) { cx.fillStyle = "#050507"; cx.fillRect(0, 0, 128, 128); tex.needsUpdate = true; done && done(); return; }
      const im = new Image(); im.onload = () => { cx.drawImage(im, 0, 0, 128, 128); tex.needsUpdate = true; done && done(); }; im.src = src;
    };
    return g;
  },
  joystick(p) { // an analog thumbstick: black square base, a stem and a big dished cap; a white ring is printed on the board around it
    const g = new T.Group(); const d = p.d || 24, base = p.base || 22; const blk = mat("#1d1d20", { roughness: 0.6 }), cap = mat("#2c2c31", { roughness: 0.82 });
    const rg = new T.Mesh(new T.RingGeometry(d * 0.6, d * 0.64, 72), mat(COL.silk, { roughness: 0.5 })); rg.position.z = 0.03; g.add(rg);
    const plate = new T.Mesh(new T.ExtrudeGeometry(roundedRect(base, base, 3), { depth: 5, bevelEnabled: true, bevelThickness: 0.4, bevelSize: 0.4, bevelSegments: 2 }), blk); plate.position.z = 0.1; g.add(plate);
    g.add(cyl(3.2, 7, blk, 0, 0, 5, 20));
    const dome = new T.Mesh(new T.SphereGeometry(d / 2, 40, 20, 0, Math.PI * 2, 0, Math.PI / 2), cap); dome.scale.set(1, 0.5, 1); dome.rotation.x = Math.PI / 2; dome.position.z = 12; g.add(dome);
    g.add(cyl(d / 2, 1.6, cap, 0, 0, 10.4, 40)); g.add(cyl(d * 0.28, 0.2, mat("#1a1a1d", { roughness: 0.9 }), 0, 0, 12 + d * 0.25 - 0.1, 24));
    return g;
  },
  tact(p) { // a large tactile switch: black body, steel cover with four dimples, a round black plunger
    const g = new T.Group(); const w = p.w || 12;
    g.add(box(w + 1, w + 1, 1.4, mat("#1b1b1d", { roughness: 0.6 })));
    g.add(box(w - 0.6, w - 0.6, 3.4, mat(COL.silver), 0, 0, 1.4));
    for (const sx of [-1, 1]) for (const sy of [-1, 1]) g.add(cyl(0.7, 0.15, mat(COL.silverDark), sx * (w / 2 - 1.5), sy * (w / 2 - 1.5), 4.8, 10));
    g.add(cyl(w * 0.36, 3.2, mat("#202023", { roughness: 0.55 }), 0, 0, 4.8, 28)); g.add(cyl(w * 0.12, 0.1, mat("#0b0b0c"), 0, 0, 8.0, 14));
    return g;
  },
  buzzer(p) { // a round magnetic buzzer can with its sound hole on top
    const g = new T.Group(); const d = p.d || 12, h = p.h || 8.5; const blk = mat("#17171a", { roughness: 0.5 });
    g.add(cyl(d / 2, h, blk, 0, 0, 0, 40)); g.add(cyl(d / 2 + 0.25, 1.0, blk, 0, 0, 0, 40));
    g.add(cyl(d * 0.34, 0.04, mat("#232327", { roughness: 0.7 }), 0, 0, h, 30)); g.add(cyl(d * 0.09, 0.12, mat("#050506"), 0, 0, h + 0.02, 16));
    g.add(cyl(0.9, 0.05, mat(COL.silk), -d * 0.28, d * 0.2, h + 0.04, 12));
    return g;
  },
  mic(p) { // an electret microphone: silver can with a dark cloth grille
    const g = new T.Group(); const d = p.d || 9.6;
    g.add(cyl(d / 2, 3.4, mat(COL.silver), 0, 0, 0, 36)); g.add(cyl(d * 0.4, 0.06, mat("#3a3a3e", { roughness: 0.95 }), 0, 0, 3.42, 30));
    return g;
  },
  rgbled(p) { // a small white LED package with a red, a green and a blue die that glow when a step lights it
    const g = new T.Group(); g.add(box(3.4, 3.4, 1.3, mat("#ece8dc", { roughness: 0.4 })));
    [["#ff3020", -0.8], ["#2fe06b", 0], ["#3a6bff", 0.8]].forEach(([c, x]) => g.add(box(0.6, 1.0, 0.2, emissive(c), x, 0, 1.3)));
    return g;
  },
  ring(p) { const g = new T.Group(); const d = p.d || 2.4; g.add(cyl(d / 2, 0.08, mat(COL.ring), 0, 0, 0, 24)); g.add(cyl(d * 0.22, 0.1, mat(COL.hole), 0, 0, 0.02, 16)); if (p.name) g.userData.pinName = p.name; return g; },
  standoff() { const g = new T.Group(); g.add(cyl(1.9, 0.1, mat(COL.silver), 0, 0, 0, 24)); g.add(cyl(1.1, 0.12, mat(COL.hole), 0, 0, 0.05, 16)); return g; },
  microusb() { const g = new T.Group(); g.add(box(7.5, 5.5, 2.6, mat(COL.silver), 0, 0.6, 0)); g.add(box(6.6, 1.0, 1.7, mat(COL.chip), 0, 3.2, 0.45)); g.add(box(7.0, 0.6, 0.3, mat(COL.pin), 0, -2.6, 0)); return g; },
  button(p) { // a tactile switch; side: left or right puts the plunger out through the board edge
    const g = new T.Group(); const w = p.w || 6, h = p.h || 4;
    g.add(box(w, h, 3.0, mat(COL.button)));
    if (p.side === "left") g.add(box(1.4, h * 0.45, 1.6, mat(COL.buttonCap), -w / 2 - 0.7, 0, 0.7));
    else if (p.side === "right") g.add(box(1.4, h * 0.45, 1.6, mat(COL.buttonCap), w / 2 + 0.7, 0, 0.7));
    else g.add(cyl(Math.min(w, h) * 0.3, 1.0, mat(COL.buttonCap), 0, 0, 3.0, 16));
    return g;
  },
  jumper2() { const g = new T.Group(); g.add(box(5.2, 2.6, 2.5, mat(COL.header))); g.add(box(4.4, 2.2, 3.2, mat(COL.shunt), 0, 0, 2.5)); return g; },
  header(p) { // male pins, black base, optional shunts: "all", "top", "bottom", "middle-h"; pitch defaults to 2.54 mm
    const g = new T.Group(); const c = p.cols || 1, r = p.rows || 1, PITCH = p.pitch || 2.54, W = c * PITCH, H = r * PITCH;
    g.add(box(W, H, 2.5, mat(COL.header)));
    const pins = [];
    for (let i = 0; i < c; i++) for (let j = 0; j < r; j++) {
      const x = -W / 2 + PITCH / 2 + i * PITCH, y = H / 2 - PITCH / 2 - j * PITCH;
      const pin = box(0.8, 0.8, 5.2, mat(COL.pin), x, y, 2.5); pins.push(pin); g.add(pin);
    }
    const sh = p.shunts;
    const shunt = (x, y, w, h) => { const s = new T.Group(); s.add(box(w, h, 4.6, mat(p.shuntColor || COL.shunt), x, y, 2.6)); s.add(box(w - 0.6, h - 0.6, 0.3, mat(p.shuntColor ? "#3a3a3d" : COL.shuntDark), x, y, 7.2)); g.add(s); };
    if (sh === "all") for (let i = 0; i < c; i++) shunt(-W / 2 + PITCH / 2 + i * PITCH, 0, 2.3, H - 0.4);
    if (sh === "top") shunt(0, H / 2 - PITCH, 2.3, 2 * PITCH - 0.4);
    if (sh === "bottom") shunt(0, -H / 2 + PITCH, 2.3, 2 * PITCH - 0.4);
    if (sh === "middle-h") shunt(0, 0, W - 0.4, 2.3);
    if (sh === "row0") shunt(0, H / 2 - PITCH / 2, W - 0.4, 2.3);   // across the top row of a 2-wide block
    g.userData.pins = pins;
    return g;
  },
  ringheader(p) { // through-hole rings with pins in them, named: the BoosterPack headers; pitch defaults to 2.54 mm and the rings scale with it
    const g = new T.Group(); const c = p.cols || 1, r = p.rows || 1, PITCH = p.pitch || 2.54, k = Math.min(1, PITCH / 2.54), W = c * PITCH, H = r * PITCH;
    for (let i = 0; i < c; i++) for (let j = 0; j < r; j++) {
      const x = -W / 2 + PITCH / 2 + i * PITCH, y = H / 2 - PITCH / 2 - j * PITCH;
      const pin = new T.Group(); pin.position.set(x, y, 0);
      pin.add(cyl(1.0 * k, 0.08, mat(COL.ring), 0, 0, 0, 20)); pin.add(cyl(0.42 * k, 0.1, mat(COL.pinDark), 0, 0, 0.02, 12)); pin.add(box(0.8 * k, 0.8 * k, 4.6, mat(COL.pin), 0, 0, 0.05));
      const name = p.names && p.names[j] && p.names[j][i]; const no = p.pinno && p.pinno[j] && p.pinno[j][i];
      if (name) pin.userData.pinName = name; if (no) pin.userData.pinNo = no;
      pin.traverse(o => { o.userData.pinGroup = pin; });
      g.add(pin);
    }
    return g;
  },
  patcharray(p) { // a radar antenna keep-out: white-masked window with a field of via dots, copper patches and their feed lines; patches: [[x, y], ...] and feeds: [[x0, y0, x1, y1], ...] from the window center
    const g = new T.Group(); const W = p.w || 18, H = p.h || 38, S = p.size || 1.5;
    const cu = mat("#a8693a", { metalness: 0.85, roughness: 0.4 });
    g.add(box(W, H, 0.06, mat("#e4e1da", { roughness: 0.72 })));
    const pitch = p.dotPitch || 0.9, nx = Math.floor((W - 1) / pitch), ny = Math.floor((H - 1) / pitch), keep = (p.patches || []).concat(p.keepout || []);
    const spots = [];
    for (let i = 0; i < nx; i++) for (let j = 0; j < ny; j++) { const x = -W / 2 + 0.5 + (i + 0.5) * pitch, y = -H / 2 + 0.5 + (j + 0.5) * pitch; if (keep.some(k => Math.abs(k[0] - x) < 1.6 && Math.abs(k[1] - y) < 1.6)) continue; spots.push([x, y]); }
    const inst = new T.InstancedMesh(new T.CylinderGeometry(0.14, 0.14, 0.02, 6), mat("#8f8c85", { roughness: 0.6 }), spots.length); const dm = new T.Matrix4();
    spots.forEach(([x, y], i) => { dm.makeRotationX(Math.PI / 2); dm.setPosition(x, y, 0.075); inst.setMatrixAt(i, dm); }); g.add(inst);
    for (const [x, y] of p.patches || []) g.add(box(S, S, 0.07, cu, x, y, 0.06));
    for (const [x0, y0, x1, y1] of p.feeds || []) { const len = Math.hypot(x1 - x0, y1 - y0); const f = box(len + 0.3, 0.32, 0.07, cu, (x0 + x1) / 2, (y0 + y1) / 2, 0.06); f.rotation.z = Math.atan2(y1 - y0, x1 - x0); g.add(f); }
    return g;
  },
  dipswitch(p) { // an SMD DIP switch bank: n sliders at 1.27 mm stacked along y, position 1 at the bottom, ON to the left; p.on is 1 or 0 per slider
    const g = new T.Group(); const n = p.n || 6, pt = p.pitch || 1.27, W = p.w || 6.4, L = n * pt + 1.6; const slotW = W - 2.2, th = slotW - 1.5; const knobs = [];
    g.add(box(W, L, 2.2, mat("#2a2a2e", { roughness: 0.45 })));
    for (let i = 0; i < n; i++) { const y = -L / 2 + 0.8 + pt / 2 + i * pt; for (const sx of [-1, 1]) g.add(box(1.0, pt * 0.38, 0.3, mat(COL.pin), sx * (W / 2 + 0.3), y, 0)); }
    g.add(box(W - 1.4, L - 0.8, 0.06, mat("#17171a", { roughness: 0.6 }), 0, 0, 2.2));
    for (let i = 0; i < n; i++) {
      const y = -L / 2 + 0.8 + pt / 2 + i * pt; g.add(box(slotW, pt * 0.74, 0.06, mat("#0e0e10", { roughness: 0.7 }), 0, y, 2.24));
      const k = box(1.5, pt * 0.62, 0.55, mat("#f1eee6", { roughness: 0.4 }), 0, y, 2.3); const on = p.on ? p.on[i] : 1; k.position.x = (on ? -1 : 1) * th / 2; k.userData.homeX = k.position.x; knobs.push(k); g.add(k);
    }
    g.userData.knobs = knobs; g.userData.throw = th; return g;
  },
  sop(p) { // a dual-row package with leads out of the left and right sides: bw across, bl along y, n pins
    const g = new T.Group(); const bw = p.bw || 3.9, bl = p.bl || 9.9, n = (p.pins || 16) / 2, pt = p.pitch || 1.27;
    g.add(box(bw, bl, 1.5, mat(COL.soic), 0, 0, 0.15)); g.add(cyl(0.4, 0.05, mat(COL.chipLight), -bw / 2 + 0.8, bl / 2 - 0.8, 1.65, 10));
    for (let i = 0; i < n; i++) { const y = (i - (n - 1) / 2) * pt; for (const sx of [-1, 1]) g.add(box(1.0, pt * 0.42, 0.2, mat(COL.pin), sx * (bw / 2 + 0.3), y, 0.05)); }
    return g;
  },
  hdconn(p) { // a high-density board-to-board receptacle: black housing, a recessed floor and two rows of gold contacts
    const g = new T.Group(); const W = p.w || 23, H = p.h || 9, D = p.d || 5.2, nn = p.rowpins || 30;
    g.add(box(W, H, D, mat("#1b1b1e", { roughness: 0.4 }))); g.add(box(W - 2.4, H * 0.46, 0.1, mat("#0b0b0c", { roughness: 0.8 }), 0, 0, D));
    for (const sy of [-1, 1]) for (let k = 0; k < nn; k++) g.add(box(0.3, 1.1, 0.14, mat(COL.pad), -(W - 4.4) / 2 + k * (W - 4.4) / (nn - 1), sy * H * 0.15, D + 0.02));
    return g;
  },
  barrel(p) { // a DC barrel jack with its opening toward +y
    const g = new T.Group(); const W = p.w || 11.5, H = p.h || 15, D = p.d || 10;
    g.add(box(W, H, D, mat("#17171a", { roughness: 0.5 })));
    const bore = new T.Mesh(new T.CylinderGeometry(3.4, 3.4, 0.3, 28), mat("#050505", { roughness: 0.9 })); bore.position.set(0, H / 2 + 0.02, D * 0.55); g.add(bore);
    const pin = new T.Mesh(new T.CylinderGeometry(0.9, 0.9, 5, 14), mat(COL.silver)); pin.position.set(0, H / 2 - 2.4, D * 0.55); g.add(pin);
    return g;
  },
  usbplug() { // a micro-USB B plug with a short cable stub, tip toward -y; the origin is the tip, so the tip sits where the plug is placed
    const g = new T.Group(); const hood = mat("#1b1b1e", { roughness: 0.55 });
    g.add(box(6.8, 5.8, 1.8, mat(COL.silver), 0, 0, 0.5)); g.add(box(6.0, 0.5, 0.9, mat(COL.chip), 0, 0.5, 0.95));
    g.add(box(8.6, 13, 5.4, hood, 0, 5.8, 0)); g.add(box(5.0, 6, 3.6, hood, 0, 21.8, 0.9));
    const cable = new T.Mesh(new T.CylinderGeometry(1.4, 1.4, 38, 14), hood); cable.position.set(0, 46, 2.7); g.add(cable);
    return g;
  },
  person(p) { // an upright figure, feet at y = 0, facing -z, drawn from capsules in matte gray and skin tones; p.h is the height
    const g = new T.Group(); const H = p.h || 49; const cloth = mat(p.color || "#d9d4c8", { roughness: 0.85 }), skin = mat("#c4a086", { roughness: 0.7 }), dark = mat("#46526a", { roughness: 0.8 });
    const cap = (r, len, m, x, y, z, sx = 1, sz = 1, rx = 0, rz = 0) => { const c = new T.Mesh(new T.CapsuleGeometry(r * H, len * H, 6, 14), m); c.position.set(x * H, y * H, z * H); c.scale.set(sx, 1, sz); c.rotation.set(rx, 0, rz); g.add(c); return c; };
    const head = new T.Mesh(new T.SphereGeometry(0.066 * H, 20, 16), skin); head.position.set(0, 0.934 * H, -0.004 * H); head.scale.set(0.92, 1.08, 1); g.add(head);
    cap(0.026, 0.03, skin, 0, 0.862, 0);                                                  // neck
    cap(0.078, 0.2, cloth, 0, 0.725, 0, 1.45, 0.78);                                      // torso
    cap(0.082, 0.04, dark, 0, 0.55, 0, 1.25, 0.8);                                        // hips
    cap(0.04, 0.4, dark, -0.058, 0.28, -0.035, 1, 1, -0.16); cap(0.04, 0.4, dark, 0.058, 0.28, 0.035, 1, 1, 0.16);    // legs mid-stride
    cap(0.026, 0.27, cloth, -0.168, 0.68, 0.02, 1, 1, 0.08, 0.05); cap(0.026, 0.27, cloth, 0.168, 0.68, -0.02, 1, 1, -0.08, -0.05);   // arms
    for (const [x, z] of [[-0.058, -0.115], [0.058, -0.045]]) { const f = new T.Mesh(new T.BoxGeometry(0.07 * H, 0.04 * H, 0.14 * H), dark); f.position.set(x * H, 0.02 * H, z * H); g.add(f); }
    return g;
  },
  fovwedge(p) { // a radar's field of view out of the board's face (local +z): half-angles az and el, range in mm, k mm per metre; a floor line with metre ticks and numbers under it
    const g = new T.Group(); const R = p.range || 150, A = (p.az ?? 60) * Math.PI / 180, E = (p.el ?? 40) * Math.PI / 180, K = p.k || R / 6.5, HR = (p.mount || 1.5) * K;
    const clip = [new T.Plane(new T.Vector3(0, 1, 0), 1.6)];   // world floor: the wedge stops at the ground
    const ns = o => { Object.defineProperty(o, "castShadow", { get: () => false, set: () => {} }); return o; };
    const dir = (a, e) => new T.Vector3(Math.sin(a) * Math.cos(e), Math.sin(e), Math.cos(a) * Math.cos(e));
    const c = new T.Color(p.color || "#1fa8a0"), pos = [], col = [], N = 32, M = 20;
    const tri = (...vs) => { for (const v of vs) { pos.push(v.x, v.y, v.z); col.push(c.r, c.g, c.b, 0.3 - 0.22 * (v.length() / R)); } };
    const at = (i, j) => dir(-A + 2 * A * i / N, -E + 2 * E * j / M).multiplyScalar(R);
    for (let i = 0; i < N; i++) for (let j = 0; j < M; j++) { tri(at(i, j), at(i + 1, j), at(i + 1, j + 1)); tri(at(i, j), at(i + 1, j + 1), at(i, j + 1)); }
    const o = new T.Vector3(); for (let i = 0; i < N; i++) { tri(o, at(i, 0), at(i + 1, 0)); tri(o, at(i, M), at(i + 1, M)); } for (let j = 0; j < M; j++) { tri(o, at(0, j), at(0, j + 1)); tri(o, at(N, j), at(N, j + 1)); }
    const geo = new T.BufferGeometry(); geo.setAttribute("position", new T.Float32BufferAttribute(pos, 3)); geo.setAttribute("color", new T.Float32BufferAttribute(col, 4));
    g.add(ns(new T.Mesh(geo, new T.MeshBasicMaterial({ vertexColors: true, transparent: true, depthWrite: false, side: T.DoubleSide, clippingPlanes: clip }))));
    const lp = [], seg = (a, b) => lp.push(a.x, a.y, a.z, b.x, b.y, b.z);
    for (const [a, e] of [[-A, -E], [A, -E], [A, E], [-A, E]]) seg(o, dir(a, e).multiplyScalar(R));
    for (let i = 0; i < N; i++) { seg(at(i, 0), at(i + 1, 0)); seg(at(i, M), at(i + 1, M)); }
    for (let j = 0; j < M; j++) { seg(at(0, j), at(0, j + 1)); seg(at(N, j), at(N, j + 1)); }
    for (const r of p.rings || []) for (let i = 0; i < N; i++) seg(dir(-A + 2 * A * i / N, 0).multiplyScalar(r * K), dir(-A + 2 * A * (i + 1) / N, 0).multiplyScalar(r * K));
    const lg = new T.BufferGeometry(); lg.setAttribute("position", new T.Float32BufferAttribute(lp, 3));
    g.add(ns(new T.LineSegments(lg, new T.LineBasicMaterial({ color: p.line || "#0f766e", transparent: true, opacity: 0.8, depthWrite: false, clippingPlanes: clip }))));
    const tape = mat("#6b6e75", { roughness: 0.8 }), far = (p.floor || 8) * K, fl = p.floor || 8;
    const strip = ns(new T.Mesh(new T.BoxGeometry(1.0, 0.2, far), tape)); strip.position.set(0, -HR, far / 2); g.add(strip);
    for (let m = 0; m <= fl; m++) { const tk = ns(new T.Mesh(new T.BoxGeometry(m % 2 ? 4 : 7, 0.2, 0.7), tape)); tk.position.set(0, -HR, m * K); g.add(tk); }
    for (const m of p.labels || [2, 4, 8]) { const t = textPlane(`${m} m`, 3.4, "#3d4048"); t.rotation.set(-Math.PI / 2, 0, 0); t.position.set(-10, -HR + 0.15, m * K); ns(t); g.add(t); }
    return g;
  },
  boxheader(p) { const g = new T.Group(); const c = p.cols || 2, r = p.rows || 5; const W = c * 1.27 + 2.6, H = r * 1.27 + 1.2; g.add(box(W, H, 5.0, mat(COL.button))); for (let i = 0; i < c; i++) for (let j = 0; j < r; j++) g.add(cyl(0.3, 0.3, mat(COL.shunt), -W / 2 + 1.3 + 0.635 + i * 1.27, H / 2 - 0.6 - 0.635 - j * 1.27, 4.9, 8)); return g; },
  fpc(p) { // a fine-pitch connector: a light housing open at the top with the pins standing inside, named like a ringheader (cols x rows, names, pinno)
    // so a wire can end on "part.pinN" and a tap gives the pin's name; the Waveshare panel's 15-way Interface 2 is one at 1.25 mm
    const g = new T.Group(); const c = p.cols || 1, r = p.rows || 1, PITCH = p.pitch || 1.25, W = c * PITCH + (p.wall || 1.2) * 2 + 0.6, H = r * PITCH + (p.wall || 1.2) * 2 + 0.6, D = p.d || 4.5;
    const pw = Math.min(0.5, PITCH * 0.36);
    for (let i = 0; i < c; i++) for (let j = 0; j < r; j++) {   // the pins first, so children[N] is pin N (columns first, top row first)
      const x = -W / 2 + (W - c * PITCH) / 2 + PITCH / 2 + i * PITCH, y = H / 2 - (H - r * PITCH) / 2 - PITCH / 2 - j * PITCH;
      const pin = new T.Group(); pin.position.set(x, y, 0);
      pin.add(box(pw, pw, D - 0.6, mat(COL.pin), 0, 0, 0.3));
      const name = p.names && p.names[j] && p.names[j][i]; const no = p.pinno && p.pinno[j] && p.pinno[j][i];
      if (name) pin.userData.pinName = name; if (no) pin.userData.pinNo = no;
      pin.traverse(o => { o.userData.pinGroup = pin; });
      g.add(pin);
    }
    const body = mat(p.color || COL.cream, { roughness: 0.55 }); const wall = p.wall || 1.2;
    g.add(box(W, H, 0.6, body));                                                                 // floor
    g.add(box(wall, H, D, body, -W / 2 + wall / 2, 0, 0)); g.add(box(wall, H, D, body, W / 2 - wall / 2, 0, 0));   // long walls
    g.add(box(W, wall, D, body, 0, H / 2 - wall / 2, 0)); g.add(box(W, wall, D, body, 0, -H / 2 + wall / 2, 0));    // end walls
    return g;
  },
  glass(p) { // a screen: a dark bezel (w by h, d thick) with a glossy glass face and the lit area (aw by ah, offset ax, ay) a shade lighter under it
    const g = new T.Group(); const w = p.w || 92.44, h = p.h || 61, d = p.d || 3, aw = p.aw || 73.44, ah = p.ah || 48.96;
    const bezel = new T.Mesh(new T.ExtrudeGeometry(roundedRect(w, h, p.corner || 3), { depth: d - 0.3, bevelEnabled: false }), mat(p.color || "#0b0b0e", { roughness: 0.5 }));
    g.add(bezel);
    const face = new T.Mesh(new T.ExtrudeGeometry(roundedRect(w - 0.2, h - 0.2, p.corner || 3), { depth: 0.3, bevelEnabled: false }), mat("#101114", { clearcoat: 1, clearcoatRoughness: 0.08, roughness: 0.2, metalness: 0.1 }));
    face.position.z = d - 0.3; g.add(face);
    g.add(box(aw, ah, 0.04, mat(p.active || "#1a1d26", { roughness: 0.25, clearcoat: 0.8, clearcoatRoughness: 0.1 }), p.ax || 0, p.ay || 0, d));
    return g;
  },
  femaleheader(p) { const g = new T.Group(); const c = p.cols || 1, r = p.rows || 1, W = c * PITCH, H = r * PITCH; g.add(box(W, H, 7.0, mat(p.light ? COL.silverDark : COL.header))); for (let i = 0; i < c; i++) for (let j = 0; j < r; j++) g.add(box(1.0, 1.0, 0.2, mat(COL.hole), -W / 2 + PITCH / 2 + i * PITCH, H / 2 - PITCH / 2 - j * PITCH, 6.95)); return g; },
  speaker(p) { // a small enclosed loudspeaker: black plastic case, a collar, a rubber surround and a glossy cone that can move (userData.cone)
    const g = new T.Group(); const w = p.w || 24.7, h = p.h || 26.3, d = p.d || 7;
    const body = new T.Mesh(new T.ExtrudeGeometry(roundedRect(w - 1, h - 1, 2.0), { depth: d - 1, bevelEnabled: true, bevelThickness: 0.5, bevelSize: 0.5, bevelSegments: 3 }), mat("#18181b", { roughness: 0.5 }));
    body.position.z = 0.5; g.add(body);
    const ring = (ro, ri, depth, m, z) => { const s = new T.Shape(); s.absarc(0, 0, ro, 0, Math.PI * 2, false); const hole = new T.Path(); hole.absarc(0, 0, ri, 0, Math.PI * 2, true); s.holes.push(hole); const o = new T.Mesh(new T.ExtrudeGeometry(s, { depth, bevelEnabled: false, curveSegments: 40 }), m); o.position.z = z; return o; };
    g.add(ring(9.6, 7.9, 0.6, mat("#2e2e32", { roughness: 0.42 }), d));                                  // collar
    g.add(cyl(7.9, 0.1, mat("#0b0b0c", { roughness: 0.9 }), 0, 0, d - 1.6, 40));                         // dark floor under the cone
    const sur = new T.Mesh(new T.TorusGeometry(7.2, 0.8, 12, 56), mat("#111113", { roughness: 0.8 })); sur.position.z = d - 0.45; g.add(sur);   // rubber surround
    const cone = new T.Group(); cone.position.z = d - 1.1;
    const pts = [[3.0, 0.8], [4.6, 0.32], [6.5, -0.15]].map(([r, z]) => new T.Vector2(r, z));
    const lm = new T.Mesh(new T.LatheGeometry(pts, 56), mat("#222226", { roughness: 0.3, metalness: 0.2, side: T.DoubleSide })); lm.rotation.x = Math.PI / 2; cone.add(lm);
    const cap = new T.Mesh(new T.SphereGeometry(3.3, 40, 12, 0, Math.PI * 2, 0, Math.PI / 2), mat("#111114", { roughness: 0.2, metalness: 0.25 })); cap.scale.y = 0.3; cap.rotation.x = Math.PI / 2; cap.position.z = 0.75; cone.add(cap); g.add(cone);   // dust cap
    g.userData.cone = cone; g.userData.coneZ = cone.position.z;
    for (const [x, y] of [[-w / 2 + 2.3, h / 2 - 2.3], [w / 2 - 2.3, -h / 2 + 2.3]]) { g.add(cyl(1.9, 0.25, mat("#242428", { roughness: 0.45 }), x, y, d, 20)); g.add(cyl(1.4, 0.45, mat("#e9e7e0", { roughness: 0.45 }), x, y, d + 0.1, 20)); }   // mounting screws
    for (const [s, c] of [[-1, "#b3261e"], [1, "#151515"]]) {   // two soldered leads to the board
      const curve = new T.CatmullRomCurve3([new T.Vector3(s * 0.9, -h / 2 + 0.4, d * 0.45), new T.Vector3(s * 1.4, -h / 2 - 2.2, d * 0.35), new T.Vector3(s * 2.6, -h / 2 - 5, 0.5)]);
      g.add(new T.Mesh(new T.TubeGeometry(curve, 14, 0.32, 8, false), mat(c, { roughness: 0.6 })));
    }
    return g;
  },
  jack35(p) { // a 3.5 mm stereo jack lying on the board with its opening toward +y: black housing, a silver bush and bore, solder tabs and two clips
    const g = new T.Group(); const w = p.w || 10.6, h = p.h || 14.6, d = p.d || 5.8;
    g.add(box(w, h, d, mat("#1c1c1f", { roughness: 0.4 })));
    const bush = new T.Mesh(new T.CylinderGeometry(3.1, 3.1, 2.4, 32), mat(COL.silver)); bush.position.set(0, h / 2 + 0.6, d / 2); g.add(bush);
    const bore = new T.Mesh(new T.CylinderGeometry(1.9, 1.9, 0.2, 24), mat("#050505", { roughness: 0.9 })); bore.position.set(0, h / 2 + 1.75, d / 2); g.add(bore);
    for (const [x, y] of [[-1, 3.5], [1, 0.5], [-1, -4], [1, 4.5]]) g.add(box(1.3, 2.3, 0.25, mat(COL.silver), x * (w / 2 + 0.4), y, 0.2));
    for (const y of [3, -4]) g.add(box(w - 2.6, 0.9, 0.3, mat(COL.silverDark), 0, y, d));
    return g;
  },
  electret(p) { // an electret microphone capsule standing on the board: silver can, rolled lip, a cloth-covered top with the sound hole
    const g = new T.Group(); const r = (p.d || 6) / 2, H = p.h || 5;
    g.add(cyl(r, H, mat(COL.silver), 0, 0, 0, 36)); g.add(cyl(r + 0.12, 0.5, mat(COL.silver), 0, 0, H - 0.5, 36));
    g.add(cyl(r - 0.55, 0.06, mat("#1a1a1c", { roughness: 0.7 }), 0, 0, H, 36)); g.add(cyl(0.9, 0.1, mat("#050505", { roughness: 0.9 }), 0, 0, H + 0.03, 16));
    return g;
  },
  volwheel(p) { // a thumb-wheel volume control: a black knurled wheel on a small base with a metal frame, overhanging the board edge
    const g = new T.Group(); const r = (p.d || 13.6) / 2;
    g.add(box(6, 6, 1.4, mat("#2a2a2d", { roughness: 0.5 }))); g.add(box(3.2, 10, 3.2, mat(COL.silver), r - 0.2, 0, 0));
    g.add(cyl(r, 3.2, mat("#1b1b1d", { roughness: 0.55 }), 0, 0, 1.4, 56));
    for (let k = 0; k < 44; k++) { const a = k / 44 * Math.PI * 2; const rib = box(0.45, 0.9, 3.2, mat("#1b1b1d", { roughness: 0.55 }), Math.cos(a) * r, Math.sin(a) * r, 1.4); rib.rotation.z = a; g.add(rib); }
    g.add(cyl(2.0, 0.2, mat(COL.silver), 0, 0, 4.6, 24)); g.add(box(2.4, 0.4, 0.1, mat("#2a2a2d"), 0, 0, 4.8));
    return g;
  },
  tantalum(p) { const g = new T.Group(); const L = p.len || 3.5, W = p.w || 2.8; g.add(box(L, W, 1.8, mat("#d99a2b", { roughness: 0.4 }))); g.add(box(0.5, W + 0.02, 1.82, mat("#5a3d10", { roughness: 0.5 }), -L / 2 + 0.65, 0, 0)); g.add(box(0.8, W * 0.9, 0.4, mat(COL.pin), -L / 2 - 0.1, 0, 0)); g.add(box(0.8, W * 0.9, 0.4, mat(COL.pin), L / 2 + 0.1, 0, 0)); return g; },
  tssop(p) { const g = new T.Group(); const n = (p.pins || 16) / 2; g.add(box(5.0, 4.4, 1.1, mat(COL.soic))); g.add(cyl(0.3, 0.05, mat(COL.chipLight), -2.0, 1.6, 1.1, 10)); for (let i = 0; i < n; i++) { const x = (i - (n - 1) / 2) * 0.65; g.add(box(0.28, 1.0, 0.2, mat(COL.pin), x, 2.6, 0)); g.add(box(0.28, 1.0, 0.2, mat(COL.pin), x, -2.6, 0)); } return g; },
  usbplug(p) { // a USB-C cable end with its tip at the origin pointing toward -y: metal shell, black overmold, strain relief and a short cable
    const g = new T.Group(); const rub = mat("#1c1c1f", { roughness: 0.6 });
    g.add(box(8.4, 10, 2.6, mat(COL.silver), 0, 5, -1.3));
    const mold = new T.Mesh(new T.ExtrudeGeometry(roundedRect(10.2, 15, 2), { depth: 5.4, bevelEnabled: true, bevelThickness: 0.5, bevelSize: 0.5, bevelSegments: 3 }), rub); mold.position.set(0, 17.5, -3.2); g.add(mold);
    const relief = new T.Mesh(new T.CylinderGeometry(3.0, 2.2, 5, 24), rub); relief.position.set(0, 27.5, 0); g.add(relief);
    const curve = new T.CatmullRomCurve3([new T.Vector3(0, 29, 0), new T.Vector3(0, 38, 0), new T.Vector3(4, 47, -1), new T.Vector3(14, 54, -1.4)]);
    g.add(new T.Mesh(new T.TubeGeometry(curve, 24, 1.5, 10, false), rub));
    return g;
  },
};

function buildBoard(data) {
  const root = new T.Group(); const [W, H] = data.size; const thick = 1.6;
  const shape = roundedRect(W, H, data.corner || 2);
  for (const [x, y, d] of data.holes || []) { const h = new T.Path(); h.absarc(x - W / 2, y - H / 2, d / 2, 0, Math.PI * 2, true); shape.holes.push(h); }
  const pcb = new T.Mesh(new T.ExtrudeGeometry(shape, { depth: thick, bevelEnabled: false }), mat(data.color || "#a8101a", { roughness: 0.6, clearcoat: 0.3, clearcoatRoughness: 0.45 }));
  pcb.position.set(W / 2, H / 2, -thick); pcb.receiveShadow = true; pcb.castShadow = true; root.add(pcb);
  const parts = []; const byId = {};
  const place = (p, group) => {
    group.position.set(p.x, p.y, p.side === "bottom" ? -thick : 0); group.rotation.z = (p.rot || 0) * Math.PI / 180;
    if (p.side === "bottom") group.rotation.x = Math.PI;   // hangs under the board, mirrored in y
    root.add(group); return group;
  };
  // detail: "key" (default) draws only passives 1206 and larger, so the board reads at a glance; "all" draws every part in the table
  const minLen = data.detail === "all" ? 0 : 2.6;
  for (const p of data.decor || []) { const f = PARTS[p.type]; if (!f) continue; if (p.type === "chip" && (p.len || 1.6) < minLen) continue; place(p, f(p)); }
  for (const p of data.parts || []) {
    const f = PARTS[p.type]; if (!f) { console.warn("boardviz: no part type", p.type); continue; }
    const g = place(p, f(p));
    if (p.id) { g.userData.part = p; g.traverse(o => { if (!o.userData.pinGroup) o.userData.partGroup = g; }); parts.push(g); byId[p.id] = g; }
    if (p.name && p.type !== "ring") g.userData.pinName = p.name;
  }
  for (const s of data.silk || []) { const t = textPlane(s.t, s.s || 1.5, COL.silk, s.rot || 0); t.position.set(s.x, s.y, 0.03); root.add(t); }
  root.traverse(o => { if (o.isMesh && !o.userData.silk) { o.castShadow = true; o.receiveShadow = true; } });
  // lay the board flat: board y becomes world -z, board z becomes world y
  root.rotation.x = -Math.PI / 2; root.position.set(-W / 2, 0, H / 2);
  return { root, parts, byId, W, H, thick };
}
export { buildBoard, PARTS, mat, box, cyl, COL, T, emissive, textPlane };

// ---------- outline pass ----------
const OUTLINE_VS = `varying vec2 vUv; void main(){ vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }`;
const OUTLINE_FS = `
varying vec2 vUv;
uniform sampler2D tColor, tNormal, tDepth; uniform vec2 texel; uniform float thickness, depthThreshold, normalThreshold, lineOpacity; uniform vec3 lineColor;
float depthAt(vec2 uv){ return texture2D(tDepth, uv).r; }
vec3 normalAt(vec2 uv){ return texture2D(tNormal, uv).rgb * 2.0 - 1.0; }
void main(){
  vec4 col = texture2D(tColor, vUv);
  vec2 d = texel * thickness;
  float z = depthAt(vUv); vec3 n = normalAt(vUv);
  float e = 0.0;
  vec2 o[4]; o[0]=vec2(d.x,0.0); o[1]=vec2(-d.x,0.0); o[2]=vec2(0.0,d.y); o[3]=vec2(0.0,-d.y);
  // depth: a second difference along x and along y, so a sloped flat face is not an edge at any zoom or line width
  float zx1 = depthAt(vUv + o[0]), zx2 = depthAt(vUv + o[1]), zy1 = depthAt(vUv + o[2]), zy2 = depthAt(vUv + o[3]);
  float kx = abs(zx1 + zx2 - 2.0 * z), ky = abs(zy1 + zy2 - 2.0 * z);
  float zmin = max(1e-5, min(min(zx1, zx2), min(zy1, zy2)) + 1e-3);
  e += step(depthThreshold, max(kx, ky) / zmin * 400.0);
  for (int i=0;i<4;i++){ vec3 nn = normalAt(vUv + o[i]); e += step(normalThreshold, 1.0 - dot(n, nn)); }
  e = clamp(e, 0.0, 1.0) * lineOpacity;
  gl_FragColor = vec4(mix(col.rgb, lineColor, e), max(col.a, e));
  #include <colorspace_fragment>
}`;
class Outline {
  constructor(renderer, { lineColor = COL.line, thickness = 1.9 } = {}) {
    this.renderer = renderer;
    this.colorT = new T.WebGLRenderTarget(1, 1, { samples: 4 });
    this.normalT = new T.WebGLRenderTarget(1, 1, { depthTexture: new T.DepthTexture(1, 1, T.FloatType) });
    this.normalMat = new T.MeshNormalMaterial();
    this.mat = new T.ShaderMaterial({ vertexShader: OUTLINE_VS, fragmentShader: OUTLINE_FS, premultipliedAlpha: false, depthTest: false, depthWrite: false, transparent: true,
      uniforms: { tColor: { value: this.colorT.texture }, tNormal: { value: this.normalT.texture }, tDepth: { value: this.normalT.depthTexture }, texel: { value: new T.Vector2() },
        thickness: { value: thickness }, depthThreshold: { value: 0.6 }, normalThreshold: { value: 0.35 }, lineColor: { value: new T.Color(lineColor) }, lineOpacity: { value: 1.0 } } });
    this.quad = new T.Mesh(new T.PlaneGeometry(2, 2), this.mat); this.scene = new T.Scene(); this.scene.add(this.quad);
    this.cam = new T.OrthographicCamera(-1, 1, 1, -1, 0, 1);
  }
  setSize(w, h, dpr) { const W = Math.max(1, Math.round(w * dpr)), H = Math.max(1, Math.round(h * dpr)); this.colorT.setSize(W, H); this.normalT.setSize(W, H); this.mat.uniforms.texel.value.set(1 / W, 1 / H); }
  render(scene, camera) {
    const r = this.renderer; const bg = scene.background;
    r.setRenderTarget(this.colorT); r.setClearColor(0, 0); r.clear(); r.render(scene, camera);
    scene.overrideMaterial = this.normalMat; camera.layers.disable(1); r.setRenderTarget(this.normalT); r.setClearColor(0x808080, 1); r.clear(); r.render(scene, camera); camera.layers.enable(1); scene.overrideMaterial = null;
    r.setRenderTarget(null); r.setClearColor(0, 0); r.clear(); r.render(this.scene, this.cam);
    scene.background = bg;
  }
}

// ---------- the view ----------
export class BoardViz {
  constructor(el, data, opts = {}) {
    this.el = el; this.data = data; this.opts = opts;
    el.classList.add("bviz-on");
    this.canvas = document.createElement("canvas"); el.appendChild(this.canvas);
    this.labelLayer = document.createElement("div"); this.labelLayer.className = "bviz-labels"; el.appendChild(this.labelLayer);
    this.style = STYLE;
    this.renderer = new T.WebGLRenderer({ canvas: this.canvas, antialias: STYLE === "real", alpha: true, powerPreference: "high-performance" });
    this.renderer.setClearColor(0, 0); this.renderer.outputColorSpace = T.SRGBColorSpace;
    if (STYLE === "real") { this.renderer.toneMapping = T.NeutralToneMapping; this.renderer.toneMappingExposure = 0.95; }
    this.renderer.shadowMap.enabled = true; this.renderer.shadowMap.type = T.VSMShadowMap; this.renderer.shadowMap.autoUpdate = false; this.renderer.shadowMap.needsUpdate = true;
    this.scene = new T.Scene();
    this.camera = new T.OrthographicCamera(-1, 1, 1, -1, 1, 900);
    this.outline = STYLE === "drawn" ? new Outline(this.renderer, { thickness: opts.thickness || 1.9 }) : null;
    if (STYLE === "real") { this.scene.environment = studio(this.renderer); this.scene.environmentIntensity = 0.75; }
    this.scene.add(new T.HemisphereLight("#fffaf3", "#c2ae90", STYLE === "real" ? 0.35 : 1.1));
    const key = new T.DirectionalLight("#fff6ea", STYLE === "real" ? 1.5 : 1.6); key.position.set(-40, 120, 40); this.scene.add(key);
    const sun = new T.DirectionalLight("#ffffff", 0.001); sun.position.set(70, 104, -34); sun.castShadow = true; sun.shadow.mapSize.set(2048, 2048);
    Object.assign(sun.shadow.camera, { left: -220, right: 220, top: 220, bottom: -220, near: 20, far: 400 }); sun.shadow.radius = 9; sun.shadow.blurSamples = 16; sun.shadow.bias = -4e-4;
    this.scene.add(sun); this.scene.add(sun.target);
    const ground = new T.Mesh(new T.PlaneGeometry(900, 900), new T.ShadowMaterial({ color: COL.ground, opacity: 0.16, depthWrite: false }));
    ground.rotation.x = -Math.PI / 2; ground.position.y = -1.7; ground.receiveShadow = true; ground.layers.set(1); this.camera.layers.enable(1); this.scene.add(ground);   // layer 1: outside the outline pass
    const b = buildBoard(data); this.board = b.root; this.parts = b.parts; this.byId = b.byId; this.W = b.W; this.H = b.H; this.scene.add(this.board);
    this.pickRoots = [this.board]; this.extent = null; this.onFrame = null;
    this.view = { azimuth: opts.azimuth ?? -1.15, elevation: opts.elevation ?? 0.6, zoom: 1, target: new T.Vector3(0, 0, 0) };
    this.raycaster = new T.Raycaster(); this.ndc = new T.Vector2(); this.hovered = null; this.selected = null;
    this.dirty = true; this.size = { w: 1, h: 1 }; this.fit = 1;
    this.setupInput(); this.resize();
    if (typeof ResizeObserver !== "undefined") new ResizeObserver(() => this.resize()).observe(el);
    this.loop = this.loop.bind(this); requestAnimationFrame(this.loop);
  }
  resize() {
    const w = this.el.clientWidth || 600, h = this.el.clientHeight || 420; const dpr = Math.min(2, window.devicePixelRatio || 1);
    this.size = { w, h }; this.renderer.setPixelRatio(dpr); this.renderer.setSize(w, h, false); if (this.outline) this.outline.setSize(w, h, dpr);
    this.fitBoard();
    this.dirty = true;
  }
  fitBoard() { // px per mm so the board's projected box fills the view with a margin
    const { w, h } = this.size; this.fit = 1; const z = this.view.zoom; this.view.zoom = 1; this.placeCamera(); this.view.zoom = z;
    const e = this.extent || [0, 0, this.W, this.H]; const pts = [];   // extent: [x0, y0, x1, y1] in board mm
    for (const x of [e[0] - this.W / 2, e[2] - this.W / 2]) for (const y of [-2, 10]) for (const yy of [e[1], e[3]]) pts.push(new T.Vector3(x, y, this.H / 2 - yy).project(this.camera));
    const xs = pts.map(p => p.x * w / 2), ys = pts.map(p => p.y * h / 2);
    const ew = Math.max(...xs) - Math.min(...xs), eh = Math.max(...ys) - Math.min(...ys);
    this.fit = Math.min(w / ew, h / eh) * 0.94;
  }
  placeCamera() {
    const { azimuth: a, elevation: e, zoom, target: t } = this.view; const s = zoom * this.fit; const { w, h } = this.size; const c = this.camera;
    c.left = -w / 2 / s; c.right = w / 2 / s; c.top = h / 2 / s; c.bottom = -h / 2 / s; c.updateProjectionMatrix();
    c.position.set(t.x + Math.sin(a) * Math.cos(e) * 320, t.y + Math.sin(e) * 320, t.z + Math.cos(a) * Math.cos(e) * 320); c.lookAt(t); c.updateMatrixWorld();
  }
  setupInput() {
    const el = this.el; let drag = null;
    el.addEventListener("pointerdown", ev => { drag = { x: ev.clientX, y: ev.clientY, moved: false, a: this.view.azimuth, e: this.view.elevation, button: ev.button, shift: ev.shiftKey, t: this.view.target.clone() }; el.setPointerCapture(ev.pointerId); });
    el.addEventListener("pointermove", ev => {
      this.pointer = { x: ev.clientX, y: ev.clientY };
      if (drag) {
        const dx = ev.clientX - drag.x, dy = ev.clientY - drag.y; if (Math.hypot(dx, dy) > 3) drag.moved = true;
        if (drag.button === 2 || drag.shift) { const s = this.view.zoom * this.fit; const right = new T.Vector3(Math.cos(this.view.azimuth), 0, -Math.sin(this.view.azimuth)); const fwd = new T.Vector3(-Math.sin(this.view.azimuth), 0, -Math.cos(this.view.azimuth)); this.view.target.copy(drag.t).addScaledVector(right, -dx / s).addScaledVector(fwd, dy / s / Math.max(0.3, Math.sin(this.view.elevation))); }
        else { this.view.azimuth = drag.a - dx * 0.008; this.view.elevation = Math.min(1.5, Math.max(0.25, drag.e + dy * 0.006)); }
        this.dirty = true;
      } else this.hoverAt(ev.clientX, ev.clientY);
    });
    const up = ev => { if (drag && !drag.moved) this.clickAt(ev.clientX, ev.clientY); drag = null; };
    el.addEventListener("pointerup", up); el.addEventListener("pointercancel", () => { drag = null; });
    el.addEventListener("pointerleave", () => { this.pointer = null; this.setHovered(null); });
    el.addEventListener("contextmenu", ev => ev.preventDefault());
    el.addEventListener("wheel", ev => { ev.preventDefault(); this.view.zoom = Math.min(8, Math.max(0.5, this.view.zoom * Math.exp(-ev.deltaY * 0.0015))); this.dirty = true; }, { passive: false });
    let pinch = null;
    el.addEventListener("touchstart", ev => { if (ev.touches.length === 2) pinch = { d: Math.hypot(ev.touches[0].clientX - ev.touches[1].clientX, ev.touches[0].clientY - ev.touches[1].clientY), z: this.view.zoom }; }, { passive: true });
    el.addEventListener("touchmove", ev => { if (pinch && ev.touches.length === 2) { const d = Math.hypot(ev.touches[0].clientX - ev.touches[1].clientX, ev.touches[0].clientY - ev.touches[1].clientY); this.view.zoom = Math.min(8, Math.max(0.5, pinch.z * d / pinch.d)); this.dirty = true; ev.preventDefault(); } }, { passive: false });
    el.addEventListener("touchend", () => { pinch = null; });
    el.style.touchAction = "none";
  }
  pick(cx, cy) {
    const r = this.el.getBoundingClientRect(); this.ndc.set(((cx - r.left) / r.width) * 2 - 1, -((cy - r.top) / r.height) * 2 + 1);
    this.raycaster.setFromCamera(this.ndc, this.camera);
    const hits = this.raycaster.intersectObjects(this.pickRoots, true);
    for (const h of hits) { const o = h.object; if (o.userData.silk) continue; return { pin: o.userData.pinGroup, part: o.userData.partGroup, point: h.point }; }
    return null;
  }
  hoverAt(x, y) { const h = this.pick(x, y); this.setHovered(h && (h.pin || h.part) ? h : null); this.el.style.cursor = h && (h.pin || h.part) ? "pointer" : "grab"; }
  setHovered(h) { const key = h ? (h.pin || h.part) : null; if (key === (this.hovered && (this.hovered.pin || this.hovered.part))) return; this.hovered = h; this.dirty = true; }
  clickAt(x, y) { const h = this.pick(x, y); this.select(h && (h.pin || h.part) ? h : null); }
  select(h) { this.selected = h; this.dirty = true; if (this.opts.onSelect) this.opts.onSelect(h ? this.describe(h) : null); }
  describe(h) {
    if (h.pin) { const u = h.pin.userData; const part = h.pin.parent && h.pin.parent.userData.part; const where = part ? part.label : ""; return { kind: "pin", title: (u.pinNo ? `Pin ${u.pinNo}` : u.pinName) + (u.pinNo && u.pinName ? ` (${u.pinName})` : ""), text: where ? `On ${where}.` : "", name: u.pinName }; }
    const p = h.part.userData.part; return { kind: "part", title: p.title || p.label, text: p.text || "", name: p.label };
  }
  worldOf(obj) { const v = new T.Vector3(); obj.getWorldPosition(v); return v; }
  toScreen(v) { const p = v.clone().project(this.camera); return { x: (p.x + 1) / 2 * this.size.w, y: (1 - p.y) / 2 * this.size.h }; }
  drawLabels() {
    const L = this.labelLayer; L.textContent = "";
    const pill = (obj, text, cls, color) => { const s = this.toScreen(obj.isVector3 ? obj : this.worldOf(obj)); const d = document.createElement("div"); d.className = "bviz-pill " + cls; d.style.left = s.x + "px"; d.style.top = s.y + "px"; const i = document.createElement("i"); if (color) i.style.background = color; d.appendChild(i); const sp = document.createElement("span"); sp.textContent = text; d.appendChild(sp); L.appendChild(d); };
    this.drawTags(this.extraLabels || []);
    if (this.pinLabelFn) this.drawPinLabels(this.pinLabelFn());
    if (this.opts.showLabels !== false && !this.selected) for (const g of this.labelParts || this.parts) { if (!g.visible || !g.parent) continue; const p = g.userData.part; if (!p.label) continue; if (p.key) pill(g, p.label, "bviz-static"); }
    const h = this.selected || this.hovered;
    if (h) { const d = this.describe(h); pill(h.pin || h.part, d.title, this.selected ? "bviz-sel" : "bviz-hover"); }
    else if (this.focusObj && this.focusObj.visible && !(this.extraLabels || []).length) pill(this.focusObj, this.focusLabel || "", "bviz-sel");
  }
  // wire-end tags: a dot on the hole, the text in a column beside the group with a leader line, so five ways 5 mm apart still read
  drawTags(tags) {
    if (!tags.length) return; const L = this.labelLayer; const { w, h } = this.size;
    const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg"); svg.setAttribute("class", "bviz-leaders"); svg.setAttribute("width", w); svg.setAttribute("height", h); L.appendChild(svg);
    const groups = { near: tags.filter(t => t.cls !== "bviz-far"), far: tags.filter(t => t.cls === "bviz-far") };
    const anchors = tags.map(t => this.toScreen(t.at));   // every dot on the picture: no tag box may cover one, its own or another's
    const bw = it => it.t.text.length * 6.6 + 18; const placed = [];
    const hits = (it, x) => { const x0 = it.right ? x : x - bw(it), x1 = x0 + bw(it), y0 = it.ly - 12, y1 = it.ly + 12; return anchors.some(a => a.x > x0 - 10 && a.x < x1 + 10 && a.y > y0 - 10 && a.y < y1 + 10); };
    for (const list of Object.values(groups)) {
      if (!list.length) continue;
      const items = list.map(t => ({ t, s: this.toScreen(t.at) }));
      const cx = items.reduce((a, i) => a + i.s.x, 0) / items.length; const right = cx < w * 0.62;   // column on the emptier side
      const xs = items.map(i => i.s.x), ys = items.map(i => i.s.y); const horizontal = Math.max(...xs) - Math.min(...xs) > Math.max(...ys) - Math.min(...ys);
      items.sort((a, b) => horizontal ? a.s.x - b.s.x : a.s.y - b.s.y);   // read the holes left to right or top to bottom, so leaders do not cross
      const gap = 24, mid = items.reduce((a, i) => a + i.s.y, 0) / items.length, span = (items.length - 1) * gap;
      items.forEach((it, k) => { it.right = right; it.ly = mid - span / 2 + k * gap; it.lx = (right ? Math.max(...items.map(i => i.s.x)) + 48 : Math.min(...items.map(i => i.s.x)) - 48); });
      for (const it of items) {
        // push the box outward along its leader until it clears every dot; flip to the other side when it would leave the picture
        for (let n = 0; n < 8 && hits(it, it.lx); n++) it.lx += it.right ? 28 : -28;
        if (it.right ? it.lx + bw(it) > w - 4 : it.lx - bw(it) < 4) { it.right = !it.right; it.lx = it.s.x + (it.right ? 48 : -48); for (let n = 0; n < 8 && hits(it, it.lx); n++) it.lx += it.right ? 28 : -28; }
        for (let n = 0; n < 4 && hits(it, it.lx); n++) it.ly += n % 2 ? -26 : 26;
        const box = (q) => { const x0 = q.right ? q.lx : q.lx - bw(q); return { x0, x1: x0 + bw(q), y0: q.ly - 12, y1: q.ly + 12 }; };
        const clash = (q) => { const c = box(q); return placed.some(o => c.x0 < o.x1 + 4 && c.x1 > o.x0 - 4 && c.y0 < o.y1 + 4 && c.y1 > o.y0 - 4); };
        for (let n = 0; n < 6 && clash(it); n++) it.ly += (n % 2 ? -1 : 1) * 26 * (1 + (n >> 1));   // tags of the two groups never sit on each other
        placed.push(box(it));
        it.lx = Math.max(it.right ? 4 : bw(it) + 4, Math.min(it.lx, it.right ? w - bw(it) - 4 : w - 4));
      }
      for (const it of items) {
        const dot = document.createElement("div"); dot.className = "bviz-tagdot"; dot.style.left = it.s.x + "px"; dot.style.top = it.s.y + "px"; if (it.t.color) dot.style.background = it.t.color; L.appendChild(dot);
        const line = document.createElementNS("http://www.w3.org/2000/svg", "path"); line.setAttribute("d", `M${it.s.x},${it.s.y} L${it.lx + (it.right ? -6 : 6)},${it.ly}`); svg.appendChild(line);
        const lab = document.createElement("div"); lab.className = "bviz-taglab" + (it.right ? "" : " bviz-left"); lab.style.left = it.lx + "px"; lab.style.top = it.ly + "px"; lab.textContent = it.t.text; L.appendChild(lab);
      }
    }
  }
  // small always-on pin names ("J2 20 GND") on the pins a wire has been drawn to, so a reader can check a pin without counting
  drawPinLabels(list) {
    const L = this.labelLayer, active = new Set((this.extraLabels || []).map(t => t.text));
    for (const p of list) {
      if ([...active].some(t => t.replace(/ pin /, " ").replace(/[()]/g, "") === p.text)) continue;   // the step's own tag already names it
      const s = this.toScreen(p.at); const d = document.createElement("div"); d.className = "bviz-pinlab"; d.style.left = s.x + "px"; d.style.top = s.y + "px"; d.textContent = p.text; L.appendChild(d);
    }
  }
  loop() {
    if (this.onFrame && this.onFrame(performance.now())) this.dirty = true;
    if (this.dirty) {
      this.dirty = false; this.placeCamera();
      if (this.outline) { this.outline.mat.uniforms.thickness.value = Math.min(2.0, Math.max(1.0, 0.45 * this.view.zoom * this.fit)); this.outline.render(this.scene, this.camera); }
      else this.renderer.render(this.scene, this.camera);
      this.drawLabels();
    }   // ink width follows px per mm: thin when far, full when close
    requestAnimationFrame(this.loop);
  }
  resetView() { this.view.azimuth = this.opts.azimuth ?? -1.15; this.view.elevation = this.opts.elevation ?? 0.6; this.view.zoom = 1; this.view.target.set(0, 0, 0); this.fitBoard(); this.dirty = true; }
  topView() { this.view.azimuth = -Math.PI / 2; this.view.elevation = 1.5; this.dirty = true; }   // straight down, keeping where you were looking and how close: the pin-finding view
}

export function webglOk() { try { const c = document.createElement("canvas"); return !!(c.getContext("webgl2") || c.getContext("webgl")); } catch { return false; } }

// Mount every <div class="bviz" data-board=...> whose data sits in a <script type="application/json" id="bviz-<board>">.
export function mountAll() {
  for (const el of document.querySelectorAll(".bviz[data-board]")) {
    const id = el.dataset.board; const src = document.getElementById("bviz-" + id); if (!src) continue;
    if (!webglOk()) { el.classList.add("bviz-nogl"); continue; }
    const data = JSON.parse(src.textContent);
    const card = el.parentElement.querySelector(".bviz-card");
    const viz = new BoardViz(el, data, { onSelect: d => { if (!card) return; if (!d) { card.hidden = true; return; } card.hidden = false; card.textContent = ""; const b = document.createElement("b"); b.textContent = d.title; card.appendChild(b); if (d.text) { const q = document.createElement("p"); q.textContent = d.text; card.appendChild(q); } } });
    const photo = el.parentElement.querySelector(".bviz-photo");
    el.parentElement.querySelectorAll("[data-bviz]").forEach(b => b.addEventListener("click", () => {
      const a = b.dataset.bviz; if (a === "reset") viz.resetView(); if (a === "top") viz.topView();
      if (a === "photo" && photo) { const show = photo.hidden; photo.hidden = !show; el.hidden = show; b.textContent = show ? "Drawing" : "Photo"; if (!show) viz.resize(); }
    }));
    el.bviz = viz;
  }
}
