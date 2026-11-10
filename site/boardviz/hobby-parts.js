// Hobbyist breadboard parts for the demo scenes (site/boardviz/scene.js): a half-size breadboard with addressable holes and the
// small parts of a starter kit. Built from the same box/cylinder/material helpers as the board parts (boardviz.js), so the
// realistic style and the drawn style (?bviz=drawn) both work. Sizes are millimeters from the parts' public datasheet
// drawings (2.54 mm pitch). Every thing has an origin at its contact surface: z 0 is the top of the breadboard, or the table
// for a part that stands on its own (servo, TM1637, power module). Pins are places a wire can end: "led1.a", "bb.h12".
// tools/boardviz/check.py reads the PINS table below, so keep it plain JSON.
import { T, mat, box, cyl, COL, emissive, textPlane } from "./boardviz.js";

// PINS: part type -> pin name -> [x, y, z] in the part's own frame, mm. The breadboard's holes are computed (holePos).
const PINS = {"led":{"a":[1.27,0,0],"k":[-1.27,0,0]},"resistor":{"p1":[-3.81,0,0],"p2":[3.81,0,0]},"button":{"p1":[-2.54,3.81,0],"p2":[2.54,3.81,0],"p3":[-2.54,-3.81,0],"p4":[2.54,-3.81,0]},"pot":{"p1":[-5.08,0,0],"w":[0,0,0],"p3":[5.08,0,0]},"buzzer":{"plus":[-3.81,0,0],"minus":[3.81,0,0]},"servo":{"gnd":[-49,-2.54,6.5],"vcc":[-49,0,6.5],"sig":[-49,2.54,6.5]},"hcsr04":{"vcc":[-3.81,0,0],"trig":[-1.27,0,0],"echo":[1.27,0,0],"gnd":[3.81,0,0]},"dht11":{"vcc":[-2.54,0,0],"data":[0,0,0],"gnd":[2.54,0,0]},"oled":{"gnd":[-3.81,0,0],"vcc":[-1.27,0,0],"scl":[1.27,0,0],"sda":[3.81,0,0]},"pir":{"vcc":[-2.54,0,0],"out":[0,0,0],"gnd":[2.54,0,0]},"tm1637":{"clk":[-19,-3.81,10],"dio":[-19,-1.27,10],"vcc":[-19,1.27,10],"gnd":[-19,3.81,10]},"powermod":{"plus":[0,0,0],"minus":[0,2.54,0]},"drv8833":{"vm":[-7.62,0,0],"gnd":[-5.08,0,0],"stby":[-2.54,0,0],"ain1":[0.0,0,0],"ain2":[2.54,0,0],"ao1":[5.08,0,0],"ao2":[7.62,0,0]},"capacitor":{"plus":[-1.27,0,0],"minus":[1.27,0,0]},"dcmotor":{"m1":[-15,-2.5,8],"m2":[-15,2.5,8]},"ky040":{"clk":[-13,-5.08,10],"dt":[-13,-2.54,10],"sw":[-13,0,10],"plus":[-13,2.54,10],"gnd":[-13,5.08,10]},"ws2812ring":{"vcc":[-2.54,-19,10],"gnd":[0,-19,10],"din":[2.54,-19,10]},"ws2812strip":{"v5":[-44,-2.54,9],"din":[-44,0,9],"gnd":[-44,2.54,9]},"relay":{"coilp":[-3.81,0,0],"coiln":[-1.27,0,0],"com":[1.27,0,0],"no":[3.81,0,0]},"transistor":{"e":[-2.54,0,0],"b":[0,0,0],"c":[2.54,0,0]},"diode":{"a":[-1.27,0,0],"k":[1.27,0,0]},"lamp":{"plus":[-13,-2.5,6],"minus":[-13,2.5,6]},"supply":{"plus":[14,-6,21],"minus":[14,6,21]},"mpu6050":{"vcc":[0,0,0],"gnd":[2.54,0,0],"scl":[5.08,0,0],"sda":[7.62,0,0]},"chassis":{"ml1":[-26,37.5,14],"ml2":[-26,40.5,14],"mr1":[-26,-40.5,14],"mr2":[-26,-37.5,14]},"panel35":{"vcc":[-38,17.78,10],"v33":[-38,15.24,10],"gnd":[-38,12.7,10],"miso":[-38,10.16,10],"mosi":[-38,7.62,10],"sclk":[-38,5.08,10],"sdcs":[-38,2.54,10],"lcdcs":[-38,0.0,10],"lcddc":[-38,-2.54,10],"lcdrst":[-38,-5.08,10],"lcdbl":[-38,-7.62,10],"tpsda":[-38,-10.16,10],"tpscl":[-38,-12.7,10],"tpint":[-38,-15.24,10],"tprst":[-38,-17.78,10]},"pickpad":{"pad":[0,0,10]}};
// end PINS

const PITCH = 2.54, BB = { L: 82.5, W: 54.6, H: 8.5 };
const ROWS = "abcdefghij";

// Breadboard holes. Terminal strips: column 1 to 30, rows a to e below the center channel and f to j above it; the five holes of
// one column and half are joined. Power rails tp, tm (top, outer and inner) and bp, bm (bottom): 25 holes each, joined end to end.
export function holePos(name) {
  let m = /^([a-j])(\d{1,2})$/.exec(name);
  if (m) {
    const col = +m[2], i = ROWS.indexOf(m[1]); if (col < 1 || col > 30) return null;
    return [(col - 15.5) * PITCH, i < 5 ? -(13.97 - i * PITCH) : 3.81 + (i - 5) * PITCH];
  }
  m = /^(tp|tm|bp|bm)(\d{1,2})$/.exec(name);
  if (m) {
    const k = +m[2] - 1; if (k < 0 || k > 24) return null;
    return [-35.56 + (k + Math.floor(k / 5)) * PITCH, { tp: 24.13, tm: 21.59, bp: -24.13, bm: -21.59 }[m[1]]];
  }
  return null;
}
export function holeNames() {
  const out = []; for (const r of ROWS) for (let c = 1; c <= 30; c++) out.push(r + c);
  for (const r of ["tp", "tm", "bp", "bm"]) for (let k = 1; k <= 25; k++) out.push(r + k);
  return out;
}

// a thing whose pins are places: wires name them as "<thing id>.<pin>"; scene.js asks userData.place(name)
function placeable(g, lookup) {
  const cache = {};
  g.userData.place = name => {
    if (cache[name]) return cache[name];
    const xyz = lookup(name); if (!xyz) throw new Error("no place " + name);
    const o = new T.Group(); o.position.set(xyz[0], xyz[1], xyz[2] || 0); g.add(o); cache[name] = o; return o;
  };
  return g;
}
const tableOf = type => name => PINS[type][name];

const metal = (c = "#c6c4bf") => mat(c, { metalness: 1, roughness: 0.32 });
const leg = (x, y, z0, z1) => box(0.6, 0.6, z1 - z0, mat(COL.pin), x, y, z0);
const header = (w = 10.2, d = 2.54) => box(w, d, 2.5, mat(COL.header));
const flatText = (text, size, color, x, y, z) => {   // text on a part that stands upright: faces -y
  const t = textPlane(text, size, color); t.rotation.x = Math.PI / 2; t.position.set(x, y, z); return t;
};

// 4-band resistor colors from a value in ohms: two digits, a power of ten, gold tolerance
const BAND = ["#15151a", "#7a3b10", "#c8281e", "#e8761a", "#e8d21a", "#2a8a3a", "#2a4fc0", "#7a3aa8", "#8a8a8a", "#f0f0f0"];
function bands(ohms) {
  const s = String(Math.round(ohms)); const d1 = +s[0], d2 = s.length > 1 ? +s[1] : 0, mult = Math.max(0, s.length - 2);
  return [BAND[d1], BAND[d2], BAND[mult], "#c9a227"];
}

export const HOBBY = {
  breadboard(p) { // half-size, 400 points: 30 columns x 10 rows and four rails of 25
    const g = new T.Group(); const { L, W, H } = BB;
    g.add(box(L, W, H, mat("#ebe7dc", { roughness: 0.55 })));
    g.add(box(L - 2, 3.0, 0.08, mat("#b9b4a4", { roughness: 0.7 }), 0, 0, H));                       // the center channel
    for (const [y, c] of [[26.3, "#c8281e"], [19.4, "#2a5bd7"], [-26.3, "#c8281e"], [-19.4, "#2a5bd7"]]) g.add(box(L - 12, 0.5, 0.08, mat(c, { roughness: 0.6 }), 0, y, H));   // rail stripes
    const names = holeNames(); const inst = new T.InstancedMesh(new T.BoxGeometry(1.15, 1.15, 0.12), mat(COL.hole, { roughness: 0.95 }), names.length); const d = new T.Object3D();
    const map = {};
    names.forEach((n, i) => { const [x, y] = holePos(n); map[n] = [x, y]; d.position.set(x, y, H + 0.02); d.updateMatrix(); inst.setMatrixAt(i, d.matrix); });
    inst.instanceMatrix.needsUpdate = true; g.add(inst);
    const ink = "#8a8676";
    for (const c of [1, 5, 10, 15, 20, 25, 30]) { const x = (c - 15.5) * PITCH; for (const y of [17.7, -17.7]) { const t = textPlane(String(c), 1.7, ink); t.position.set(x, y, H + 0.05); g.add(t); } }
    ROWS.split("").forEach((r, i) => { const t = textPlane(r, 1.7, ink); t.position.set(-39.2, holePos(r + "1")[1], H + 0.05); g.add(t); });
    for (const [y, s, c] of [[24.13, "+", "#c8281e"], [21.59, "-", "#2a5bd7"], [-24.13, "+", "#c8281e"], [-21.59, "-", "#2a5bd7"]]) { const t = textPlane(s, 1.9, c); t.position.set(-39.2, y, H + 0.05); g.add(t); }
    g.userData.holes = map; g.userData.top = H;
    return placeable(g, name => { const h = holePos(name); return h && [h[0], h[1], H + 0.1]; });
  },
  led(p) { // 5 mm through-hole LED, long leg is the anode (+); color is the lens
    const g = new T.Group(); const col = p.color || "#ff2a1a";
    for (const [x, c] of [[1.27, 0], [-1.27, 1]]) g.add(leg(x, 0, -3, 3.2));
    g.add(cyl(2.95, 1.0, mat(col, { roughness: 0.3 }), 0, 0, 3.0, 32));
    g.add(cyl(2.5, 4.2, emissiveAt(col, 0.5), 0, 0, 4.0, 32));
    const dome = new T.Mesh(new T.SphereGeometry(2.5, 32, 14, 0, Math.PI * 2, 0, Math.PI / 2), emissiveAt(col, 0.5)); dome.rotation.x = Math.PI / 2; dome.position.z = 8.2; g.add(dome);
    g.add(box(0.2, 1.8, 1.2, mat("#ffffff", { roughness: 0.6 }), -2.55, 0, 4.2));   // the flat edge on the cathode side
    return placeable(g, tableOf("led"));
  },
  resistor(p) { // 1/4 W axial, four color bands from its value in ohms
    const g = new T.Group(); const z = 2.9; const bd = bands(p.ohms || 220);
    for (const s of [-1, 1]) { g.add(leg(s * 3.81, 0, -3, z + 0.3)); g.add(box(1.0, 0.6, 0.6, mat(COL.pin), s * 3.4, 0, z - 0.3)); }
    const body = new T.Mesh(new T.CylinderGeometry(1.15, 1.15, 6.3, 20), mat("#d9c39a", { roughness: 0.55 })); body.rotation.z = Math.PI / 2; body.position.z = z; g.add(body);
    [-2.2, -1.2, -0.2, 2.2].forEach((x, i) => { const b = new T.Mesh(new T.CylinderGeometry(1.2, 1.2, 0.5, 20), mat(bd[i], { roughness: 0.5 })); b.rotation.z = Math.PI / 2; b.position.set(x, 0, z); g.add(b); });
    return placeable(g, tableOf("resistor"));
  },
  button(p) { // 6 mm tactile switch: legs 5.08 mm along the channel, 7.62 mm across it. Opposite corners are the two sides of the switch.
    const g = new T.Group(); const cap = p.color || "#2a2a2e";
    for (const sx of [-1, 1]) for (const sy of [-1, 1]) { g.add(leg(sx * 2.54, sy * 3.81, -3, 1.2)); g.add(box(0.8, 1.2, 0.6, mat(COL.pin), sx * 2.54, sy * 3.2, 0.6)); }
    g.add(box(6, 6, 3.4, mat("#1f1f22", { roughness: 0.6 }), 0, 0, 0.5));
    g.add(box(5.5, 5.5, 0.4, metal("#b9b9bc"), 0, 0, 3.9));
    g.add(cyl(1.75, 1.7, mat(cap, { roughness: 0.5 }), 0, 0, 4.3, 24));
    return placeable(g, tableOf("button"));
  },
  pot(p) { // 10 k rotary potentiometer: three pins 5.08 mm apart, the middle one is the wiper
    const g = new T.Group();
    for (const x of [-5.08, 0, 5.08]) g.add(leg(x, 0, -3, 1.8));
    g.add(box(12, 9.5, 0.9, mat("#23252b", { roughness: 0.6 }), 0, 0, 1.2));
    g.add(cyl(4.7, 4.2, metal("#bdbdc0"), 0, 0, 2.1, 32));
    g.add(cyl(3.0, 5.0, metal("#d4d4d8"), 0, 0, 6.3, 24));
    const knob = new T.Group(); knob.position.z = 8.0;
    knob.add(cyl(5.2, 7.5, mat("#2b2d33", { roughness: 0.45 }), 0, 0, 0, 36)); knob.add(box(0.7, 4.2, 0.12, mat(COL.silk), 0, 2.2, 7.5));
    knob.rotation.z = (p.angle || 0) * Math.PI / 180; g.add(knob); g.userData.knob = knob;
    return placeable(g, tableOf("pot"));
  },
  buzzer(p) { // 12 mm passive buzzer: a tone from a square wave, no oscillator inside
    const g = new T.Group();
    for (const x of [-3.81, 3.81]) g.add(leg(x, 0, -3, 2.2));
    g.add(cyl(6.0, 1.0, mat("#2f8a4f", { roughness: 0.5 }), 0, 0, 1.6, 36));
    g.add(cyl(6.0, 7.5, mat("#1c1c1f", { roughness: 0.55 }), 0, 0, 2.6, 36));
    g.add(cyl(1.1, 0.12, mat(COL.hole), 0, 0, 10.1, 16));
    const t = textPlane("+", 2.2, COL.silk); t.position.set(-3.6, 2.6, 10.15); g.add(t);
    return placeable(g, tableOf("buzzer"));
  },
  servo(p) { // SG90 9 g: shaft up, horn on top, a 3-wire lead (brown ground, red power, orange signal) ending in a 3-socket plug
    const g = new T.Group(); const blue = mat(p.color || "#2a62c8", { roughness: 0.45 }), white = mat("#f1f0ea", { roughness: 0.5 });
    g.add(box(22.5, 12.2, 16, blue)); g.add(box(32.5, 12.2, 2.0, blue, 0, 0, 16)); g.add(box(22.5, 12.2, 4.7, blue, 0, 0, 18));
    for (const sx of [-1, 1]) g.add(cyl(1.1, 0.1, mat(COL.hole), sx * 14.2, 0, 18.02, 14));
    g.add(cyl(6.1, 2.4, blue, 5.5, 0, 22.7, 32)); g.add(cyl(3.0, 1.6, blue, -1.0, 0, 22.7, 24));
    g.add(cyl(2.4, 2.8, metal(), 5.5, 0, 25.1, 20));
    const horn = new T.Group(); horn.position.set(5.5, 0, 26.0);
    horn.add(cyl(3.6, 2.4, white, 0, 0, 0, 24)); horn.add(box(26, 3.4, 1.4, white, 0, 0, 0.9)); horn.add(cyl(1.1, 0.4, metal(), 0, 0, 2.4, 12));
    horn.rotation.z = (p.horn || 0) * Math.PI / 180; g.add(horn); g.userData.horn = horn;
    [["#6b3a1a", -2.54], ["#c8281e", 0], ["#e8761a", 2.54]].forEach(([c, y]) => g.add(box(30, 1.0, 1.0, mat(c, { roughness: 0.6 }), -26.25, y, 6.0)));
    g.add(box(8, 8.2, 2.8, mat("#1d1d1f", { roughness: 0.55 }), -45, 0, 5.1));
    g.userData.wireOut = new T.Group(); g.userData.wireOut.position.set(-49, 0, 6.5); g.add(g.userData.wireOut);
    return placeable(g, tableOf("servo"));
  },
  hcsr04(p) { // ultrasonic ranger, 45 x 20 mm, standing on its 4-pin header with the two transducers facing out. Also the RCWL-1601 (variant "rcwl").
    const g = new T.Group(); const rcwl = p.variant === "rcwl"; const pcb = mat(rcwl ? "#1a5aa6" : "#1f66bb", { roughness: 0.5 });
    g.add(header());
    for (const x of [-3.81, -1.27, 1.27, 3.81]) g.add(leg(x, 0, -3, 3));
    g.add(box(45, 1.6, 20, pcb, 0, 0, 2.5));
    for (const sx of [-1, 1]) {
      const can = new T.Mesh(new T.CylinderGeometry(8.2, 8.2, 12, 32), metal("#b9b9bd")); can.position.set(sx * 13, -6.8, 13.0); g.add(can);
      const face = new T.Mesh(new T.CylinderGeometry(6.6, 6.6, 0.4, 32), mat("#26282c", { roughness: 0.6 })); face.position.set(sx * 13, -12.7, 13.0); g.add(face);
    }
    g.add(flatText(rcwl ? "RCWL-1601" : "HC-SR04", 2.0, COL.silk, 0, -0.85, 6.5));
    g.add(box(8, 0.8, 3, mat(COL.chip), -8, 1.2, 6)); g.add(box(7, 0.8, 3, mat(COL.chip), 8, 1.2, 6)); if (!rcwl) g.add(box(11, 0.8, 4.5, metal(), 0, 1.2, 12));
    const back = flatText("VCC TRIG ECHO GND", 1.0, COL.silk, 0, 0.85, 3.6); back.rotation.x = -Math.PI / 2; g.add(back);
    return placeable(g, tableOf("hcsr04"));
  },
  dht11(p) { // 3-pin DHT11 module: blue sensor body with slots on a small board, standing on its header. Pin order differs between makers; this one reads + OUT -.
    const g = new T.Group();
    g.add(header(8)); for (const x of [-2.54, 0, 2.54]) g.add(leg(x, 0, -3, 3));
    g.add(box(21, 1.6, 23, mat("#1f7a45", { roughness: 0.5 }), 0, 0, 2.5));
    g.add(box(12, 5.5, 15.5, mat("#2f9bd9", { roughness: 0.5 }), 0, -3.55, 7.5));
    for (let k = 0; k < 6; k++) g.add(box(8, 0.25, 0.6, mat("#13354d"), 0, -6.35, 11.5 + k * 1.7));
    g.add(flatText("DHT11", 1.7, "#e8f4ff", 0, -6.4, 9.2));
    g.add(box(2.2, 0.8, 1.2, mat(COL.chip), 6.5, 1.2, 5)); g.add(box(2.2, 0.8, 1.2, mat(COL.chip), -6.5, 1.2, 5));
    return placeable(g, tableOf("dht11"));
  },
  oled(p) { // 0.96 inch SSD1306 module, 27 x 27 mm, 4 pins (GND VCC SCL SDA). lines: what the screen shows.
    const g = new T.Group();
    g.add(header()); for (const x of [-3.81, -1.27, 1.27, 3.81]) g.add(leg(x, 0, -3, 3));
    g.add(box(27.3, 1.6, 27.8, mat("#1a4f9e", { roughness: 0.5 }), 0, 0, 2.5));
    g.add(box(26.0, 0.8, 17.2, mat("#08080a", { roughness: 0.2 }), 0, -1.2, 11.5));
    const lines = p.lines || []; lines.forEach((s, i) => g.add(flatText(s, i === 0 ? 2.5 : 2.0, "#a8e0ff", 0, -1.63, 16.3 - i * 4.2)));
    g.add(flatText("GND VCC SCL SDA", 1.05, COL.silk, 0, -0.85, 5.0));
    for (const sx of [-1, 1]) g.add(box(2.4, 0.2, 2.4, mat(COL.hole), sx * 11.7, -0.82, 28.0));
    return placeable(g, tableOf("oled"));
  },
  pir(p) { // HC-SR501: white lens on a 32 x 24 mm board, three pins (VCC OUT GND) on one edge, two orange trimmers. table: true lays it on the table, pins up, for jumper wires.
    const g = new T.Group(); const z0 = p.table ? 0 : 2.5, up = p.table ? 1.6 : 0;
    if (p.table) { g.add(header(8)).children.at(-1).position.z = 1.6; for (const x of [-2.54, 0, 2.54]) g.add(box(0.64, 0.64, 8.5, mat(COL.pin), x, 0, 1.6)); }
    else { g.add(header(8)); for (const x of [-2.54, 0, 2.54]) g.add(leg(x, 0, -3, 3)); }
    g.add(box(32, 24, 1.6, mat("#2b8a5a", { roughness: 0.5 }), 0, 10, z0));
    g.add(cyl(11.6, 2.2, metal("#b9b9bd"), 0, 10, z0 + 1.6, 40));
    const dome = new T.Mesh(new T.SphereGeometry(11.2, 36, 16, 0, Math.PI * 2, 0, Math.PI / 2), mat("#f6f2e4", { roughness: 0.4 })); dome.rotation.x = Math.PI / 2; dome.scale.set(1, 0.75, 1); dome.position.set(0, 10, z0 + 3.8); g.add(dome);
    for (const sx of [-1, 1]) g.add(box(6, 5, 5.0, mat("#e08a1e", { roughness: 0.5 }), sx * 11, 20, z0 + 1.6));
    g.add(box(5, 3, 1.4, mat(COL.chip), 0, 0.2, z0 + 1.6)); g.add(box(6, 2.6, 1.6, mat(COL.header), -10.5, 3, z0 + 1.6));
    return placeable(g, p.table ? (name => PINS.pir[name] && [PINS.pir[name][0], 0, 10.1]) : tableOf("pir"));
  },
  tm1637(p) { // 4-digit seven-segment module, lying on the table, header (CLK DIO VCC GND) at its left end; its pins are the wire ends
    const g = new T.Group();
    g.add(box(42, 24, 1.6, mat("#1d4f9a", { roughness: 0.5 })));
    g.add(box(30, 14, 7.2, mat("#17181b", { roughness: 0.4 }), 3, 0, 1.6));
    const t = textPlane(p.digits || "88:88", 6.5, "#ff4a38"); t.position.set(3, 0, 8.85); g.add(t);
    g.add(box(2.54, 10.2, 2.5, mat(COL.header), -19, 0, 1.6)); for (const y of [-3.81, -1.27, 1.27, 3.81]) g.add(box(0.64, 0.64, 8.5, mat(COL.pin), -19, y, 1.6));
    return placeable(g, tableOf("tm1637"));
  },
  powermod(p) { // the kit's breadboard power module: plugs onto a pair of rails at the end of the breadboard (plus on the outer rail), barrel jack at the far end
    const g = new T.Group();
    g.add(leg(0, 0, -3, 3)); g.add(leg(0, PITCH, -3, 3)); g.add(box(5.2, 2.54 + 2.54, 2.5, mat(COL.header), 0, PITCH / 2, 0));
    g.add(box(52, 33, 1.6, mat("#2b2f35", { roughness: 0.5 }), -24, 8, 2.5));
    g.add(box(13, 9, 10.5, mat("#18181a", { roughness: 0.5 }), -45, 8, 4.1)); g.add(cyl(2.0, 0.2, mat(COL.hole), -50.9, 8, 7.5, 16));
    g.add(box(6.5, 6, 2.6, mat(COL.chip), -18, 4, 4.1)); g.add(box(6.5, 6, 2.6, mat(COL.chip), -18, 17, 4.1));
    g.add(box(9, 4.5, 3.4, mat("#dcdcdc", { roughness: 0.5 }), -33, 22, 4.1));
    const t = textPlane(p.silk || "5 V", 3.6, COL.silk); t.position.set(-8, 8, 4.15); g.add(t);
    return placeable(g, tableOf("powermod"));
  },
  drv8833(p) { // DRV8833 dual motor driver carrier, standing on a 7-pin header (VM GND STBY AIN1 AIN2 AO1 AO2 here; the order differs between makers)
    const g = new T.Group();
    g.add(header(17.8)); for (let k = 0; k < 7; k++) g.add(leg((k - 3) * 2.54, 0, -3, 3));
    g.add(box(21, 1.6, 17, mat("#b3262b", { roughness: 0.5 }), 0, 0, 2.5));
    g.add(box(6.5, 0.8, 5, mat(COL.chip), 0, -1.2, 8)); g.add(flatText("DRV8833", 1.8, COL.silk, 0, -1.65, 14.5));
    g.add(box(5, 0.8, 3.4, metal("#b8b8bc"), -6, -1.2, 13.5)); g.add(box(3, 0.8, 3, mat("#c9a227"), 6.5, -1.2, 8.5));
    g.add(flatText("VM GND STBY AIN1 AIN2 AO1 AO2", 1.0, COL.silk, 0, -0.85, 4.2));
    return placeable(g, tableOf("drv8833"));
  },
  capacitor(p) { // 10 uF electrolytic, standing: the silver stripe marks the minus leg
    const g = new T.Group();
    for (const x of [-1.27, 1.27]) g.add(leg(x, 0, -3, 2));
    g.add(cyl(3.2, 7.5, mat("#1d3f9e", { roughness: 0.45 }), 0, 0, 2, 28)); g.add(cyl(3.0, 0.2, metal("#c6c4bf"), 0, 0, 9.5, 28));
    g.add(box(0.3, 2.6, 6, mat("#d8d8dc"), 3.2, 0, 2.6));
    return placeable(g, tableOf("capacitor"));
  },
  dcmotor(p) { // small hobby DC motor: metal can lying on the table, shaft out of the front (it spins), two solder tabs at the back
    const g = new T.Group(); const z = 7.5; const hz = (r, l, m, x) => { const c = new T.Mesh(new T.CylinderGeometry(r, r, l, 28), m); c.rotation.z = Math.PI / 2; c.position.set(x, 0, z); return c; };
    g.add(hz(7.5, 22, metal("#b9b9bd"), 0)); g.add(hz(7.6, 4, mat("#2b2d33"), -12)); g.add(hz(2.6, 3, metal("#d4d4d8"), 12.5));
    const shaft = new T.Group(); shaft.position.set(14, 0, z); const s = hz(1.1, 10, metal("#d4d4d8"), 5); shaft.add(s); shaft.add(box(1.2, 8, 1.2, mat("#f1f0ea"), 8, 0, -0.6)); g.add(shaft); g.userData.rotor = shaft;
    for (const y of [-2.5, 2.5]) g.add(box(5, 1.4, 0.5, metal("#c9a227"), -15, y - 0.7, z));
    return placeable(g, tableOf("dcmotor"));
  },
  ky040(p) { // KY-040 rotary encoder module, lying on the table: knob with a push switch, five pins (CLK DT SW + GND) in a header at one end
    const g = new T.Group();
    g.add(box(30, 19, 1.6, mat("#1d4f9a", { roughness: 0.5 })));
    g.add(box(12.5, 13.5, 6.5, metal("#c6c4bf"), 3, 0, 1.6)); g.add(cyl(3.2, 7, metal("#d4d4d8"), 3, 0, 8.1, 24));
    g.add(cyl(7, 9, mat("#2b2d33", { roughness: 0.45 }), 3, 0, 10, 40)); g.add(box(0.8, 4, 0.12, mat(COL.silk), 3, 3, 19));
    g.add(box(2.54, 13, 2.5, mat(COL.header), -13, 0, 1.6)); [-5.08, -2.54, 0, 2.54, 5.08].forEach(y => g.add(box(0.64, 0.64, 8.5, mat(COL.pin), -13, y, 1.6)));
    const t = textPlane("KY-040", 2.2, COL.silk); t.position.set(9, -7.6, 1.65); g.add(t);
    return placeable(g, tableOf("ky040"));
  },
  ws2812ring(p) { // 8-LED WS2812 ring module on a round board, three pins (VCC GND DIN) on a tab, lying on the table
    const g = new T.Group(); const n = 8, R = 12.5; const cols = ["#ff2a1a", "#ff9a1a", "#e8d21a", "#2fa84f", "#1ab5c8", "#2a6bd7", "#8a4fd0", "#e04a9a"];
    g.add(cyl(16, 1.6, mat("#1f1f23", { roughness: 0.5 }))); g.add(box(10, 6, 1.6, mat("#1f1f23", { roughness: 0.5 }), 0, -17, 0)); g.add(cyl(6.5, 0.12, mat(COL.hole), 0, 0, 1.6, 32));
    for (let k = 0; k < n; k++) { const a = k / n * Math.PI * 2, x = R * Math.cos(a), y = R * Math.sin(a); const b = box(5, 5, 1.4, mat("#f4f4ee", { roughness: 0.5 }), x, y, 1.6); b.rotation.z = a; g.add(b); g.add(cyl(1.4, 0.5, emissiveAt(cols[k], 0.5), x, y, 3.0, 16)); }
    g.add(box(8, 2.54, 2.5, mat(COL.header), 0, -19, 1.6)); [-2.54, 0, 2.54].forEach(x => g.add(box(0.64, 0.64, 8.5, mat(COL.pin), x, -19, 1.6)));
    return placeable(g, tableOf("ws2812ring"));
  },
  ws2812strip(p) { // a short length of WS2812B strip (5 V, LEDs every 16.7 mm on a thin tape), lying on the table; 3 pads (5V DIN GND) at the input end
    const g = new T.Group(); const cols = ["#ff2a1a", "#e8d21a", "#2fa84f", "#2a6bd7", "#8a4fd0"];
    g.add(box(96, 10, 0.5, mat("#ece8d8", { roughness: 0.6 })));
    for (let k = 0; k < 5; k++) { g.add(box(5, 5, 1.4, mat("#f4f4ee", { roughness: 0.5 }), -28 + k * 16.7, 0, 0.5)); g.add(cyl(1.4, 0.5, emissiveAt(cols[k], 0.5), -28 + k * 16.7, 0, 1.9, 16)); }
    g.add(box(8, 8, 0.3, mat("#c9a227"), -42, 0, 0.5)); [-2.54, 0, 2.54].forEach(y => g.add(box(0.64, 0.64, 8.5, mat(COL.pin), -44, y, 0.5)));
    return placeable(g, tableOf("ws2812strip"));
  },
  relay(p) { // 5 V relay on a 2.54 mm pin grid: blue cube; two coil pins, then the common (COM) and normally-open (NO) contact. Check the map printed on yours.
    const g = new T.Group();
    for (const x of [-3.81, -1.27, 1.27, 3.81]) g.add(leg(x, 0, -3, 3));
    g.add(box(19, 15.5, 15.5, mat("#2a62c8", { roughness: 0.4 }), 0, 1, 2.5)); g.add(box(15, 0.2, 8, mat("#e9e9e4"), 0, -6.8, 9));
    g.add(flatText("5 V RELAY", 1.7, "#1d1d1b", 0, -7.0, 12)); g.add(flatText("COIL COIL COM NO", 1.0, "#1d1d1b", 0, -7.0, 8.5));
    return placeable(g, tableOf("relay"));
  },
  transistor(p) { // 8050 NPN in a TO-92 case: flat face toward you, pins E B C from the left
    const g = new T.Group();
    for (const x of [-2.54, 0, 2.54]) g.add(leg(x, 0, -3, 3));
    g.add(cyl(2.4, 5, mat("#1c1c1f", { roughness: 0.55 }), 0, 0, 3, 20)); g.add(box(4.2, 0.4, 4.6, mat("#26262a"), 0, -1.9, 3.2));
    g.add(flatText("8050", 1.2, COL.silk, 0, -2.15, 5.2));
    return placeable(g, tableOf("transistor"));
  },
  diode(p) { // 1N4001, leads bent in to fit holes 2.54 mm apart: the silver band is the cathode (k), the side that points to the plus
    const g = new T.Group(); const z = 3.4;
    for (const s of [-1, 1]) { g.add(leg(s * 1.27, 0, -3, z + 0.3)); g.add(box(1.0, 0.6, 0.6, mat(COL.pin), s * 0.95, 0, z - 0.3)); }
    const body = new T.Mesh(new T.CylinderGeometry(1.35, 1.35, 3.6, 20), mat("#1b1b1e", { roughness: 0.55 })); body.rotation.z = Math.PI / 2; body.position.z = z; g.add(body);
    const band = new T.Mesh(new T.CylinderGeometry(1.4, 1.4, 0.7, 20), metal("#d8d8dc")); band.rotation.z = Math.PI / 2; band.position.set(1.1, 0, z); g.add(band);
    return placeable(g, tableOf("diode"));
  },
  lamp(p) { // small 12 V LED lamp: a frosted dome on a round base with two lead tabs; lights when the scene switches it on
    const g = new T.Group();
    g.add(cyl(9, 3, metal("#b9b9bd"), 0, 0, 0, 32)); g.add(cyl(7, 3, mat("#26282c"), 0, 0, 3, 32));
    const dome = new T.Mesh(new T.SphereGeometry(8, 32, 14, 0, Math.PI * 2, 0, Math.PI / 2), emissiveAt("#ffe9a8", 0.12)); dome.rotation.x = Math.PI / 2; dome.position.z = 6; g.add(dome);
    for (const y of [-2.5, 2.5]) { g.add(box(6, 1.4, 0.5, metal("#c9a227"), -11, y - 0.7, 5.5)); g.add(box(0.64, 0.64, 3, mat(COL.pin), -13, y, 5.5)); }
    return placeable(g, tableOf("lamp"));
  },
  supply(p) { // a power source on the table: a dark case with a red plus and a black minus terminal on top. silk: the label (12 V, 4 x AA).
    const g = new T.Group(); const w = p.w || 44;
    g.add(box(w, 28, 18, mat(p.color || "#1d1d20", { roughness: 0.5 })));
    const t = textPlane(p.silk || "12 V", 6, "#ffffff"); t.position.set(-6, 0, 18.1); g.add(t);
    g.add(cyl(2.8, 3, mat("#c8281e", { roughness: 0.4 }), 14, -6, 18, 20)); g.add(cyl(2.8, 3, mat("#3a3a3d", { roughness: 0.4 }), 14, 6, 18, 20));
    return placeable(g, tableOf("supply"));
  },
  mpu6050(p) { // GY-521 module with the MPU6050 chip, standing on its 8-pin header; the first four (VCC GND SCL SDA) are used, the other four stay empty
    const g = new T.Group(); const hd = header(20.3); hd.position.x = 8.89; g.add(hd);
    for (let k = 0; k < 4; k++) g.add(leg(k * 2.54, 0, -3, 3));
    g.add(box(20.5, 1.6, 16, mat("#2a4fc0", { roughness: 0.5 }), 8.89, 0, 2.5));
    g.add(box(4.2, 0.8, 4.2, mat(COL.chip), 8.9, -1.2, 9)); g.add(flatText("MPU6050", 1.6, COL.silk, 8.9, -1.65, 14.5));
    g.add(flatText("VCC GND SCL SDA XDA XCL AD0 INT", 0.8, COL.silk, 8.9, -0.85, 4.2));
    return placeable(g, tableOf("mpu6050"));
  },
  chassis(p) { // two-wheel robot chassis, front toward +x: a flat plate between two geared motors, a wheel on each, a ball caster at the front; lead tabs at the back
    const g = new T.Group(); const yel = mat("#d9b24a", { roughness: 0.45 }), blk = mat("#1d1d20", { roughness: 0.6 });
    g.add(box(100, 64, 3, yel, 0, 0, 14));
    for (const s of [-1, 1]) {
      g.add(box(40, 14, 16, mat("#e5c13a", { roughness: 0.5 }), -10, s * 39, 6));
      const wh = new T.Mesh(new T.CylinderGeometry(22, 22, 10, 36), blk); wh.position.set(-10, s * 51, 22); g.add(wh);
      const hub = new T.Mesh(new T.CylinderGeometry(9, 9, 10.4, 24), mat("#c6c4bf")); hub.position.set(-10, s * 51, 22); g.add(hub);
    }
    for (const y of [-40.5, -37.5, 37.5, 40.5]) g.add(box(4, 1.4, 0.5, metal("#c9a227"), -26, y - 0.7, 13.5));
    const ball = new T.Mesh(new T.SphereGeometry(5, 18, 12), metal("#d4d4d8")); ball.position.set(42, 0, 9); g.add(ball); g.add(cyl(1.5, 6, metal("#b9b9bd"), 42, 0, 9, 12));
    return placeable(g, tableOf("chassis"));
  },
  panel35(p) { // 3.5 inch 320 x 480 touch panel module, 83 x 55 mm, glass up on the table; the 15-pin header runs down the left edge, VCC at the top
    const g = new T.Group(); const nm = ["VCC", "3V3", "GND", "MISO", "MOSI", "SCLK", "SD_CS", "LCD_CS", "LCD_DC", "LCD_RST", "LCD_BL", "TP_SDA", "TP_SCL", "TP_INT", "TP_RST"];
    g.add(box(83.4, 55.2, 1.6, mat("#1d4f9a", { roughness: 0.5 })));
    g.add(box(70, 49, 2.2, mat("#0b0b0e", { roughness: 0.15 }), 6, 0, 1.6)); g.add(box(62, 43, 0.1, mat("#16203a", { roughness: 0.2 }), 6, 0, 3.8));
    g.add(box(2.54, 38.1, 2.5, mat(COL.header), -38, 0, 1.6));
    nm.forEach((s, k) => { const y = (7 - k) * 2.54; g.add(box(0.64, 0.64, 8.5, mat(COL.pin), -38, y, 1.6)); const t = textPlane(s, 1.3, COL.silk); t.position.set(-34, y, 1.65); g.add(t); });
    return placeable(g, tableOf("panel35"));
  },
  pickpad(p) { // not a part: a yellow flag that says this pin is picked at the bench; a wire's far end lands on its post
    const g = new T.Group(); const y = mat("#f2c230", { roughness: 0.5 });
    g.add(box(8, 8, 1, y)); g.add(cyl(0.9, 10, y, 0, 0, 1, 12)); g.add(box(26, 0.5, 8, y, 8, 0, 8));
    g.add(flatText("PICK AT THE BENCH", 1.6, "#1d1d1b", 8, -0.35, 11));
    return placeable(g, tableOf("pickpad"));
  },
};

// lens material for the LED: dim when off, bright when the scene lights it (scene.js sets emissiveIntensity)
function emissiveAt(color, level) { const m = emissive(color); m.emissiveIntensity = level; return m; }
