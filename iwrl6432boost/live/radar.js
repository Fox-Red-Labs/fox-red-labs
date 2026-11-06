// Browser side of the IWRL6432BOOST room view: frame parser, Web Serial link, spoken calibration.
// Same frame layout as radar_live.py (mmWave L SDK 05.05.04.02 source, GTRACK_3D build).
"use strict";
const Radar = (() => {
  const MAGIC = [2, 1, 4, 3, 6, 5, 8, 7], HDR = 40, TRK = 112;

  function findMagic(b, from) {
    outer: for (let i = from; i + 8 <= b.length; i++) {
      for (let k = 0; k < 8; k++) if (b[i + k] !== MAGIC[k]) continue outer;
      return i;
    }
    return -1;
  }

  // Split a byte buffer into whole frames; returns [frames, rest]
  function split(buf) {
    const out = [];
    let off = 0;
    for (;;) {
      const i = findMagic(buf, off);
      if (i < 0) return [out, buf.slice(Math.max(off, buf.length - 7))];
      if (buf.length - i < HDR) return [out, buf.slice(i)];
      const total = new DataView(buf.buffer, buf.byteOffset + i).getUint32(12, true);
      if (total < HDR || total > 65536) { off = i + 8; continue; }
      if (buf.length - i < total) return [out, buf.slice(i)];
      out.push(buf.slice(i, i + total));
      off = i + total;
    }
  }

  function decode(fr) {
    const v = new DataView(fr.buffer, fr.byteOffset, fr.byteLength);
    const o = { frame: v.getUint32(20, true), points: [], tracks: [], presence: null, tlvs: [] };
    const ntlv = v.getUint32(32, true);
    let off = HDR, idx = null;
    for (let n = 0; n < ntlv && off + 8 <= fr.length; n++) {
      const t = v.getUint32(off, true), len = v.getUint32(off + 4, true), b = off + 8;
      o.tlvs.push(t);
      if (t === 1) {
        for (let j = 0; j + 16 <= len; j += 16)
          o.points.push([v.getFloat32(b + j, true), v.getFloat32(b + j + 4, true), v.getFloat32(b + j + 8, true), v.getFloat32(b + j + 12, true), null]);
      } else if (t === 7) {
        for (let j = 0; j * 4 + 4 <= len && j < o.points.length; j++) o.points[j][4] = v.getInt16(b + 4 * j, true) / 10;
      } else if (t === 301 && len >= 20) {
        const xu = v.getFloat32(b, true), du = v.getFloat32(b + 4, true), su = v.getFloat32(b + 8, true);
        for (let j = 20; j + 10 <= len; j += 10)
          o.points.push([v.getInt16(b + j, true) * xu, v.getInt16(b + j + 2, true) * xu, v.getInt16(b + j + 4, true) * xu, v.getInt16(b + j + 6, true) * du, v.getUint8(b + j + 8) * su]);
      } else if (t === 308) {
        for (let j = 0; j + TRK <= len; j += TRK)
          o.tracks.push({ id: v.getUint32(b + j, true), x: v.getFloat32(b + j + 4, true), y: v.getFloat32(b + j + 8, true), z: v.getFloat32(b + j + 12, true),
                          vx: v.getFloat32(b + j + 16, true), vy: v.getFloat32(b + j + 20, true), conf: v.getFloat32(b + j + TRK - 4, true) });
      } else if (t === 309) {
        idx = Array.from(fr.slice(b, b + len));
      } else if (t === 315 && len >= 1) {
        o.presence = fr[b];
      }
      off = b + len;
    }
    if (idx && idx.length === o.points.length) o.points.forEach((p, i) => p.push(idx[i] < 250 ? idx[i] : null));
    return o;
  }

  // Web Serial link: connect, send a config line by line, stream frames to onFrame
  const link = { port: null, reader: null, writer: null, text: "", onFrame: null, onLog: null };

  async function connect() {
    link.port = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x0451, usbProductId: 0xbef3 }] });
    await link.port.open({ baudRate: 115200 });
    link.writer = link.port.writable.getWriter();
    link.reader = link.port.readable.getReader();
    let buf = new Uint8Array(0);
    (async () => {
      for (;;) {
        const { value, done } = await link.reader.read();
        if (done) break;
        link.text = (link.text + new TextDecoder().decode(value)).slice(-4000);
        const nb = new Uint8Array(buf.length + value.length); nb.set(buf); nb.set(value, buf.length);
        const [frames, rest] = split(nb); buf = rest;
        frames.forEach(f => { try { link.onFrame && link.onFrame(decode(f)); } catch (e) {} });
      }
    })();
  }

  const sleep = ms => new Promise(r => setTimeout(r, ms));

  async function sendCfg(cfgText) {
    const bad = [];
    for (const raw of cfgText.split("\n")) {
      const line = raw.trim();
      if (!line || line.startsWith("%") || line.startsWith("baudRate")) continue;
      for (let attempt = 0; attempt < 2; attempt++) {  // a line sent while a frame drains can be lost: resend once
        link.text = "";
        await link.writer.write(new TextEncoder().encode(line + "\n"));
        const t0 = Date.now();
        while (Date.now() - t0 < 2000 && !/Done|rror/.test(link.text)) await sleep(20);
        if (/Done|rror/.test(link.text)) break;
      }
      if (line.startsWith("sensorStop")) await sleep(500);
      if (!/Done/.test(link.text)) bad.push(line + " -> " + link.text.trim().slice(-100));
      link.onLog && link.onLog(line + (/Done/.test(link.text) ? "  Done" : "  " + link.text.trim().slice(-60)));
    }
    return bad;
  }

  async function cli(line, ms = 1500) {
    link.text = "";
    await link.writer.write(new TextEncoder().encode(line + "\n"));
    await sleep(ms);
    return link.text;
  }

  // Spoken calibration, the same four steps as calibrate.py
  function speak(msg) {
    return new Promise(res => {
      if (!("speechSynthesis" in window)) return res();
      const u = new SpeechSynthesisUtterance(msg); u.rate = 1.0; u.onend = res; u.onerror = res;
      speechSynthesis.speak(u);
    });
  }

  function hull(pts) {
    pts = [...new Map(pts.map(p => [p.join(","), p])).values()].sort((a, b) => a[0] - b[0] || a[1] - b[1]);
    if (pts.length < 3) return pts;
    const cr = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
    const lo = [], up = [];
    for (const p of pts) { while (lo.length >= 2 && cr(lo[lo.length - 2], lo[lo.length - 1], p) <= 0) lo.pop(); lo.push(p); }
    for (const p of pts.slice().reverse()) { while (up.length >= 2 && cr(up[up.length - 2], up[up.length - 1], p) <= 0) up.pop(); up.push(p); }
    return lo.slice(0, -1).concat(up.slice(0, -1));
  }

  async function calibrate(feed, show) {
    // feed(): a function returning frames received since the last call
    const take = async (ms) => { const out = []; const t0 = Date.now(); while (Date.now() - t0 < ms) { await sleep(100); out.push(...feed()); } return out; };
    const still = f => f.tracks.length && f.tracks.every(t => Math.hypot(t.vx, t.vy) < 0.25);
    const waitFor = async (cond, need, ms, reprompt) => {
      let run = 0, t0 = Date.now(), nxt = Date.now() + 15000;
      while (Date.now() - t0 < ms) {
        for (const f of await take(300)) { run = cond(f) ? run + 1 : 0; if (run >= need) return true; }
        if (Date.now() > nxt) { show(reprompt); await speak(reprompt); nxt = Date.now() + 15000; }
      }
      return false;
    };
    const step = async (msg) => { show(msg); await speak(msg); };
    feed();
    await step("Step one. Please step behind the radar, out of its view, and stay there until you hear the next step.");
    const ok1 = await waitFor(f => !f.tracks.length, 8, 45000, "I can still see someone. Please step behind the radar or out of the room.");
    const empty = ok1 ? await take(30000) : [];
    await step("Step two. Walk slowly along the walls of the room, all the way around, as close to the walls as you can.");
    const ok2 = await waitFor(f => f.tracks.length > 0, 2, 40000, "I cannot see you yet. Walk into the room in front of the radar.");
    const walk = ok2 ? await take(45000) : [];
    await step("Step three. Go to the doorway and stand still there.");
    await waitFor(still, 6, 30000, "Please stand still in the doorway.");
    const door = await take(6000);
    await step("Step four. Sit down where people usually sit, and stay still.");
    await waitFor(still, 6, 40000, "Please sit down and stay still.");
    const seat = await take(10000);

    const spot = fs => { const by = {}; fs.forEach(f => f.tracks.forEach(t => (by[t.id] = by[t.id] || []).push([t.x, t.y])));
      const v = Object.values(by).sort((a, b) => b.length - a.length)[0]; if (!v) return null;
      return [v.reduce((s, p) => s + p[0], 0) / v.length, v.reduce((s, p) => s + p[1], 0) / v.length]; };
    const path = walk.flatMap(f => f.tracks.map(t => [Math.round(t.x * 100) / 100, Math.round(t.y * 100) / 100]));
    if (path.length < 10) { await step("I could not follow you along the walls. Please run the setup again."); return null; }
    const cells = {};
    empty.forEach(f => f.points.forEach(p => { const k = Math.floor(p[0] / 0.5) + "," + Math.floor(p[1] / 0.5); cells[k] = (cells[k] || 0) + 1; }));
    const interference = Object.entries(cells).filter(([, n]) => n / Math.max(1, empty.length) >= 0.05)
      .map(([k, n]) => { const [i, j] = k.split(",").map(Number); return { x: (i + 0.5) * 0.5, y: (j + 0.5) * 0.5, per_frame: n / empty.length }; });
    const xs = path.map(p => p[0]), ys = path.map(p => p[1]);
    const room = { outline: hull(path), door: spot(door), seat: spot(seat), interference,
                   xmin: Math.min(...xs) - 0.3, xmax: Math.max(...xs) + 0.3, ymax: Math.max(...ys) + 0.3 };
    await step("Thank you. The room is set.");
    return room;
  }

  // Replace the room lines of a config with boxes from a calibrated room
  function roomCfg(base, r) {
    const f = v => v.toFixed(1);
    let sx0 = r.xmin + 0.3, sx1 = r.xmax - 0.3, sy0 = 0.5, sy1 = r.ymax - 0.3;
    if (r.door) { if (r.door[1] > (sy0 + sy1) / 2) sy1 = Math.min(sy1, r.door[1] - 0.8);
                  else if (r.door[0] < 0) sx0 = Math.max(sx0, r.door[0] + 0.8); else sx1 = Math.min(sx1, r.door[0] - 0.8); }
    const rep = { boundaryBox: `boundaryBox ${f(r.xmin)} ${f(r.xmax)} 0 ${f(r.ymax)} -0.5 3`,
                  staticBoundaryBox: `staticBoundaryBox ${f(sx0)} ${f(sx1)} ${f(sy0)} ${f(sy1)} 0 3`,
                  presenceBoundaryBox: `presenceBoundaryBox ${f(r.xmin)} ${f(r.xmax)} 0.3 ${f(r.ymax)} 0 3` };
    return base.split("\n").map(l => rep[l.trim().split(/\s+/)[0]] || l).join("\n");
  }

  return { split, decode, connect, sendCfg, cli, calibrate, roomCfg, speak, link };
})();
if (typeof module !== "undefined") module.exports = Radar;
