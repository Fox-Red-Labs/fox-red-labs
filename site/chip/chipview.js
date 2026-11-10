// The chip view on a board page: the build you pick, its steps in plain words, and the chip drawn left to right.
// Pins and parts on the left go in, the CPU is the spine, pins and parts on the right go out. The data is data/chip-jobs/<board>.json
// (checked against the pin table by tools/chip_jobs_check.py); this file holds no per-build facts.
// Click or tap a step or a block to follow that job; click again or click the background to let go.
const JOB_COLORS = ['#B42418'];      // one hue for every job: brand red marks what is active, the list and the arrows alike (three hues on the chip: slate, red, gold)
const FLOW_COLORS = JOB_COLORS;      // the arrows: one color
const MW = 1100, MH = 640, BODY = { x: 150, y: 40, w: 800, h: 560 }, CX = 550, CY = 320;
const CORNER = { analog: [156, 50, 'Analog'], timer: [712, 50, 'Timers'], conn: [156, 508, 'Serial and pins'], sys: [712, 508, 'System'], math: [712, 508, 'System'], core: [712, 508, 'System'] };

const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const T = (x, y, txt, cls, extra) => '<text x="' + x + '" y="' + y + '" class="' + cls + '" ' + (extra || '') + '>' + esc(txt) + '</text>';
function fmtBytes(n) {   // 648 B, 3.2 kB: the site's size format
  if (n < 1000) return n + ' B';
  return (n / 1024).toFixed(1).replace(/\.0$/, '') + ' kB';
}

export function mountChip(root, data) {
  const $ = (s, r = root) => r.querySelector(s), $$ = (s, r = root) => [...r.querySelectorAll(s)];
  const WB = {};
  Object.entries(data.blocks).forEach(([id, b]) => { WB[id] = { n: b.name, s: b.sub || '', k: b.kind, short: b.short, note: b.note }; });
  const KINDS = data.kinds, STACK = data.chip.stack_bytes;
  const S = { cur: null, pin: null, hov: null, mode: null };
  const narrow = () => matchMedia('(max-width: 759px)').matches;
  const modeNow = () => S.mode || (narrow() ? 'focus' : 'whole');
  const builds = {};
  Object.entries(data.builds).forEach(([slug, b]) => {   // chains always pass through the cpu
    const B = JSON.parse(JSON.stringify(b)); B.slug = slug;
    B.jobs.forEach((j, i) => {
      j.i = i; j.c = JOB_COLORS[i % JOB_COLORS.length];
      j.chains = j.chains.map((ch) => { ch = ch.slice(); if (!ch.includes('cpu')) { if (ch[0].startsWith('part:')) ch.push('cpu'); else if (ch[0] !== 'systick') ch.unshift('cpu'); } return ch; });
    });
    builds[slug] = B;
  });
  const cur = () => builds[S.cur];
  const jobsOfBlock = (B, id) => B.jobs.filter((j) => j.chains.some((ch) => ch.includes(id))).map((j) => j.i);
  const jobSet = (B, sel) => (sel.j != null ? new Set([sel.j]) : sel.b ? new Set(jobsOfBlock(B, sel.b)) : null);
  const kindOf = (id) => (id === 'cpu' ? 'cpu' : id.startsWith('pin:') ? 'pin' : id.startsWith('part:') ? 'part' : 'blk');
  const pinText = (p) => (p.header === 'onboard' ? 'on the board' : p.header);
  function sideRows(chain) {
    const ci = chain.indexOf('cpu');
    return chain.map((id, k) => (id === 'cpu' ? { id, side: 'cpu' } : k < ci ? { id, side: 'in' } : { id, side: 'out' }));
  }
  function graph(B) {
    const nodes = new Map(), edges = [];
    B.jobs.forEach((j) => j.chains.forEach((ch) => {
      const sr = sideRows(ch);
      sr.forEach((n) => { const key = n.side + ':' + n.id; if (!nodes.has(key)) nodes.set(key, { key, id: n.id, side: n.side, js: new Set() }); nodes.get(key).js.add(j.i); });
      for (let k = 0; k < sr.length - 1; k++) {
        const a = sr[k].side + ':' + sr[k].id, b = sr[k + 1].side + ':' + sr[k + 1].id, ek = j.i + '|' + a + '>' + b;
        if (!edges.find((e) => e.k === ek)) edges.push({ k: ek, a, b, j: j.i });
      }
    }));
    return { nodes, edges };
  }
  function defs() {
    let s = '<defs><linearGradient id="cvpkg" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#383b44"/><stop offset="1" stop-color="#2b2e35"/></linearGradient>';
    FLOW_COLORS.forEach((c, i) => { s += '<marker id="cvmh' + i + '" viewBox="0 0 10 10" refX="8.5" refY="5" markerWidth="9" markerHeight="9" markerUnits="userSpaceOnUse" orient="auto"><path d="M1 1 L9 5 L1 9 z" fill="' + c + '"/></marker>'; });
    return s + '<marker id="cvmhw" viewBox="0 0 10 10" refX="8.5" refY="5" markerWidth="8" markerHeight="8" markerUnits="userSpaceOnUse" orient="auto"><path d="M1 1 L9 5 L1 9 z" fill="#8f8a80"/></marker></defs>';
  }
  function pkg() {
    let s = '<rect x="' + BODY.x + '" y="' + BODY.y + '" width="' + BODY.w + '" height="' + BODY.h + '" rx="18" class="pkg"/>'
      + '<rect x="' + (BODY.x + 7) + '" y="' + (BODY.y + 7) + '" width="' + (BODY.w - 14) + '" height="' + (BODY.h - 14) + '" rx="12" class="pkgin"/>'
      + '<path d="M' + (BODY.x + 18) + ' ' + (BODY.y + 3) + ' H' + (BODY.x + BODY.w - 18) + '" class="pkgbev"/>'
      + '<circle cx="' + (BODY.x + 24) + '" cy="' + (BODY.y + BODY.h - 24) + '" r="6" class="pkgdot"/>';
    for (let x = BODY.x + 24; x < BODY.x + BODY.w - 10; x += 20) s += '<rect class="gp" x="' + x + '" y="' + (BODY.y - 12) + '" width="9" height="12" rx="1"/><rect class="gp" x="' + x + '" y="' + (BODY.y + BODY.h) + '" width="9" height="12" rx="1"/>';
    for (let y = BODY.y + 24; y < BODY.y + BODY.h - 10; y += 20) s += '<rect class="gp" x="' + (BODY.x - 12) + '" y="' + y + '" width="12" height="9" rx="1"/><rect class="gp" x="' + (BODY.x + BODY.w) + '" y="' + y + '" width="12" height="9" rx="1"/>';
    return s + T(BODY.x + BODY.w / 2, BODY.y + BODY.h - 8, data.chip.part, 'chipname', 'text-anchor="middle"');
  }
  function bank(id, B, x, y, w, h) {   // flash as 32 cells and SRAM as 8, each cell an equal share of the chip's memory (4 kB on the G3507); the used part is lit
    const flash = id === 'flash', cells = flash ? 32 : 8, rows = flash ? 2 : 1, cols = cells / rows, gap = 2;
    const total = flash ? data.chip.flash_bytes : data.chip.ram_bytes, per = total / cells;
    const cw = (w - 16 - gap * (cols - 1)) / cols, ch = flash ? 26 : 36, y0 = y + 40;
    const used = flash ? B.flash : B.ram - STACK, stack = flash ? 0 : Math.min(B.ram, STACK);
    let s = '<g class="cblk al" data-k="mem" data-b="' + id + '" data-js="" tabindex="0"><rect class="r" x="' + x + '" y="' + y + '" width="' + w + '" height="' + h + '" rx="6" style="--d:' + KINDS.mem.color + ';--j:' + KINDS.mem.color + '"/>';
    s += T(x + 10, y + 18, flash ? 'Flash' : 'SRAM', 'bn', 'style="text-anchor:start"') + T(x + w - 10, y + 18, fmtBytes(flash ? B.flash : B.ram) + ' of ' + fmtBytes(total), 'bt', 'style="text-anchor:end"');
    for (let r = 0; r < rows; r++) {
      for (let c = 0; c < cols; c++) {
        const i = r * cols + c, cx = x + 8 + c * (cw + gap), cy = y0 + r * (ch + gap);
        s += '<rect x="' + cx + '" y="' + cy + '" width="' + cw + '" height="' + ch + '" rx="2" class="cell"/>';
        const lit = Math.max(0, Math.min(1, (used - i * per) / per));
        if (lit > 0) s += '<rect x="' + cx + '" y="' + (cy + ch * (1 - lit)) + '" width="' + cw + '" height="' + Math.max(2, ch * lit) + '" rx="2" class="cell-on"/>';
        if (!flash && stack) {
          const sl = Math.max(0, Math.min(1, (stack - (cells - 1 - i) * per) / per));
          if (sl > 0) s += '<rect x="' + cx + '" y="' + (cy + ch * (1 - sl)) + '" width="' + cw + '" height="' + Math.max(2, ch * sl) + '" rx="2" class="cell-stack"/>';
        }
      }
    }
    s += T(x + 10, y + h - 6, flash ? fmtBytes(per) + ' blocks, lit = program' : fmtBytes(per) + ' blocks, teal data, amber stack', 'bt', 'style="text-anchor:start;font-size:11px"');
    return s + '</g>';
  }

  // Straight runs only: horizontal and vertical segments on whole pixels, one elbow at most, parallel runs 6 px apart.
  const GAP = 6;
  function lanes() {
    const placed = [];   // {h: true|false, c: fixed coordinate, a, b: span}
    return (horizontal, want, lo, hi, a, b) => {
      const clash = (c) => placed.some((q) => q.h === horizontal && Math.abs(q.c - c) < GAP && Math.min(q.b, Math.max(a, b)) - Math.max(q.a, Math.min(a, b)) > 0);
      let c = Math.round(Math.max(lo, Math.min(hi, want))), best = c;
      for (let k = 0; k < 12; k++) {
        const up = Math.round(want) + Math.ceil(k / 2) * GAP * (k % 2 ? 1 : -1);
        if (up >= lo && up <= hi && !clash(up)) { best = up; break; }
        if (k === 11) best = c;
      }
      placed.push({ h: horizontal, c: best, a: Math.min(a, b), b: Math.max(a, b) });
      return best;
    };
  }
  const R = (n) => Math.round(n);
  function drawMap(B, host) {
    const { nodes, edges } = graph(B), arr = [...nodes.values()], usedIds = new Set(arr.map((n) => n.id));
    const cpu = { x: CX - 100, y: CY - 62, w: 200, h: 124 }, col = { in: [], out: [] };
    arr.forEach((n) => { if (kindOf(n.id) === 'blk' && (n.side === 'in' || n.side === 'out')) col[n.side].push(n); });
    ['in', 'out'].forEach((sd) => {
      col[sd].sort((a, b) => (a.id === 'systick' ? -1 : b.id === 'systick' ? 1 : 0));
      const n = col[sd].length, top = 172, bot = 478, step = n > 1 ? (bot - top) / (n - 1) : 0;
      col[sd].forEach((nd, i) => {
        const nj = nd.js.size; nd.w = 150 + (nj - 1) * 10; nd.h = 52 + (nj - 1) * 8; nd.cy = n > 1 ? top + i * step : CY;
        const base = 235; nd.cx = sd === 'in' ? CX - base - nd.w / 2 + 75 : CX + base + nd.w / 2 - 75;
      });
    });
    const pinsOf = (n) => edges.filter((e) => (n.side === 'in' ? e.b === n.key : e.a === n.key)).map((e) => nodes.get(n.side === 'in' ? e.a : e.b)).filter((p) => p && kindOf(p.id) === 'pin');
    const place = (list, minGap, lo, hi) => {
      list.sort((a, b) => a.cy - b.cy);
      list.forEach((p, i) => { if (i && p.cy < list[i - 1].cy + minGap) p.cy = list[i - 1].cy + minGap; });
      const ov = list.length ? list[list.length - 1].cy - hi : 0; if (ov > 0) list.forEach((p) => (p.cy -= ov));
      if (list.length && list[0].cy < lo) { const d = lo - list[0].cy; list.forEach((p) => (p.cy += d)); }
    };
    const pins = { in: [], out: [] }, parts = { in: [], out: [] };
    ['in', 'out'].forEach((sd) => col[sd].forEach((b) => {
      const ps = pinsOf(b);
      ps.forEach((p, i) => { if (p.cy == null) { p.cy = b.cy + (i - (ps.length - 1) / 2) * 36; p.w = 64; p.h = 32; p.cx = sd === 'in' ? 120 : 980; pins[sd].push(p); } });
    }));
    arr.forEach((n) => { if (kindOf(n.id) === 'pin' && n.cy == null) { n.cy = CY; n.w = 64; n.h = 32; n.cx = n.side === 'in' ? 120 : 980; pins[n.side].push(n); } });
    ['in', 'out'].forEach((sd) => place(pins[sd], 38, 70, 570));
    arr.forEach((n) => {
      if (kindOf(n.id) !== 'part') return;
      const nb = edges.filter((e) => e.a === n.key || e.b === n.key).map((e) => nodes.get(e.a === n.key ? e.b : e.a)).filter((p) => kindOf(p.id) === 'pin');
      n.cy = nb.length ? nb.reduce((s, p) => s + p.cy, 0) / nb.length : CY; n.w = 68; n.h = 34; n.cx = n.side === 'in' ? 36 : 1064; parts[n.side].push(n);
    });
    ['in', 'out'].forEach((sd) => place(parts[sd], 44, 70, 570));
    arr.forEach((n) => { if (n.id === 'cpu') { n.cx = CX; n.cy = CY; n.w = cpu.w; n.h = cpu.h; } });
    let s = '<svg viewBox="0 0 ' + MW + ' ' + MH + '" class="cm" role="img" aria-label="The chip with the blocks this build uses lit and its data flows as arrows">' + defs() + pkg();
    s += bank('flash', B, 410, 52, 280, 104) + bank('sram', B, 410, 490, 280, 104);
    s += '<path class="flow al" d="M' + CX + ' 150 L' + CX + ' ' + (cpu.y - 2) + '" marker-end="url(#cvmhw)"/><path class="flow al" d="M' + CX + ' ' + (cpu.y + cpu.h) + ' L' + CX + ' 492" marker-start="url(#cvmhw)" marker-end="url(#cvmhw)"/>';
    const slot = {};   // blocks this build does not use: small and dim in the corners, by group
    Object.keys(WB).forEach((id) => {
      if (['cpu', 'flash', 'sram'].includes(id) || usedIds.has(id)) return;
      const b = WB[id], c = CORNER[b.k] || CORNER.sys, k = c[2], i = (slot[k] = slot[k] === undefined ? 0 : slot[k] + 1);
      const x = c[0] + (i % 3) * 78, y = c[1] + 14 + Math.floor(i / 3) * 31;
      s += '<g class="cblk idle" data-k="' + b.k + '" data-b="' + id + '" data-js="" tabindex="0"><rect class="r" x="' + x + '" y="' + y + '" width="74" height="26" rx="4" style="--d:' + KINDS[b.k].color + ';--j:' + KINDS[b.k].color + '"/>' + T(x + 37, y + 17.5, b.short || b.n, 'bt idl') + '</g>';
    });
    Object.keys(CORNER).filter((k, i, a) => a.findIndex((q) => CORNER[q][2] === CORNER[k][2]) === i).forEach((k) => { const c = CORNER[k]; s += T(c[0], c[1] + 6, c[2] + ', not used here', 'bandl'); });
    const lane = lanes(), cpuN = { in: 0, out: 0 };
    s += '<g id="cv-flows">';
    edges.forEach((e) => {
      const A = nodes.get(e.a), Bn = nodes.get(e.b); if (!A || !Bn) return;
      const L2R = A.cx < Bn.cx, ax = R(L2R ? A.cx + A.w / 2 : A.cx - A.w / 2), bx = R(L2R ? Bn.cx - Bn.w / 2 : Bn.cx + Bn.w / 2);
      const rng = (N, slotN) => (N.id === 'cpu' ? [cpu.y + 12, cpu.y + cpu.h - 12] : [N.cy - N.h / 2 + 8, N.cy + N.h / 2 - 8]);
      const ra = rng(A), rb = rng(Bn), lo = Math.max(ra[0], rb[0]), hi = Math.min(ra[1], rb[1]);
      let d;
      if (lo <= hi) {   // the two edges overlap in height: one straight horizontal run
        let want = (A.id === 'cpu' || Bn.id === 'cpu') ? lo + ((A.id === 'cpu' ? cpuN.out++ : cpuN.in++) % 5) * (hi - lo) / 4 : (lo + hi) / 2;
        const y = lane(true, want, lo, hi, ax, bx);
        d = 'M' + ax + ' ' + y + ' L' + bx + ' ' + y;
      } else {   // one elbow: out along the sender's row, then up or down into the top or bottom of the receiver
        const y0 = R(Math.max(ra[0], Math.min(ra[1], A.cy))), up = y0 > Bn.cy, ty = R(up ? Bn.cy + Bn.h / 2 : Bn.cy - Bn.h / 2);
        const x = lane(false, Bn.cx, Bn.cx - Bn.w / 2 + 10, Bn.cx + Bn.w / 2 - 10, y0, ty);
        d = 'M' + ax + ' ' + y0 + ' L' + x + ' ' + y0 + ' L' + x + ' ' + ty;
      }
      s += '<path class="flow" data-j="' + e.j + '" d="' + d + '" stroke="' + FLOW_COLORS[e.j % FLOW_COLORS.length] + '" marker-end="url(#cvmh' + (e.j % FLOW_COLORS.length) + ')"/>';
    });
    s += '</g>';
    const nodeSvg = (n) => {
      const k = kindOf(n.id), x = n.cx - n.w / 2, y = n.cy - n.h / 2, js = [...n.js];
      const o = '<g class="' + (k === 'pin' ? 'pin' : k === 'part' ? 'part' : 'cblk on') + '"' + (k === 'blk' || k === 'cpu' ? ' data-k="' + WB[n.id].k + '"' : '') + ' data-b="' + n.id + '" data-js="' + js.join(',') + '" tabindex="0">';
      if (k === 'pin') {
        const nm = n.id.slice(4);
        return o + '<rect class="pr" x="' + x + '" y="' + y + '" width="' + n.w + '" height="' + n.h + '" rx="4"/>' + T(n.cx, y + 14, nm, 'bn') + T(n.cx, y + 26, pinText(B.pins[nm]).replace('on the board', 'board'), 'bt', 'style="font-size:9.5px"') + '</g>';
      }
      if (k === 'part') {
        const p = B.parts[n.id.slice(5)];
        return o + '<rect class="pt" x="' + x + '" y="' + y + '" width="' + n.w + '" height="' + n.h + '" rx="5"/>' + T(n.cx, y + 21, p.name, 'bn pn', 'style="font-size:12px"') + '</g>';
      }
      const bk = WB[n.id], c = KINDS[bk.k].color, jc = FLOW_COLORS[(js[0] || 0) % FLOW_COLORS.length], tag = (B.tags && B.tags[n.id]) || bk.s || '';
      let r = o + '<rect class="r" x="' + x + '" y="' + y + '" width="' + n.w + '" height="' + n.h + '" rx="6" style="--d:' + c + ';--j:' + jc + '"/>'
        + T(n.cx, n.id === 'cpu' ? n.cy - 6 : y + n.h / 2 - 2, bk.n, 'bn big') + T(n.cx, n.id === 'cpu' ? n.cy + 14 : y + n.h / 2 + 15, tag, 'bt');
      js.slice(0, 4).forEach((ji, q) => { r += '<circle cx="' + (x + n.w - 9 - q * 12) + '" cy="' + (y + 9) + '" r="5" fill="' + FLOW_COLORS[ji % FLOW_COLORS.length] + '" stroke="#fff" stroke-width="1.5"/>'; });
      return r + '</g>';
    };
    arr.forEach((n) => { if (n.id !== 'cpu') s += nodeSvg(n); });
    const cn = arr.find((n) => n.id === 'cpu');
    if (cn) { cn.js = new Set(B.jobs.map((j) => j.i)); s += nodeSvg(cn); }
    host.innerHTML = s + '</svg>';
  }


  // ---- just what runs: a vertical pipeline for a phone, parts and pins above the CPU going in, below it going out
  function drawFocus(B, host, W) {
    const tight = W < 520, nodes = new Map(), edges = [];
    B.jobs.forEach((j) => j.chains.forEach((ch) => {
      const ci = ch.indexOf('cpu');
      const sr = ch.map((id, k) => ({ id, side: id === 'cpu' ? 'cpu' : k < ci ? 'in' : 'out', row: id === 'cpu' ? 3 : k < ci ? 3 - (ci - k) : 3 + (k - ci) }));
      sr.forEach((n) => { const key = n.side + ':' + n.id; if (!nodes.has(key)) nodes.set(key, Object.assign({ key, js: new Set() }, n)); nodes.get(key).js.add(j.i); });
      for (let k = 0; k < sr.length - 1; k++) { const a = sr[k].side + ':' + sr[k].id, b = sr[k + 1].side + ':' + sr[k + 1].id, ek = j.i + '|' + a + '>' + b; if (!edges.find((e) => e.k === ek)) edges.push({ k: ek, a, b, j: j.i }); }
    }));
    const rows = [[], [], [], [], [], [], []]; nodes.forEach((n) => rows[n.row].push(n));
    const sizes = { part: [tight ? 70 : 132, 44], pin: [tight ? 66 : 112, 36], blk: [tight ? 100 : 150, 52], cpu: [Math.min(W - 24, 440), 72] };
    const kind = (id) => (id === 'cpu' ? 'cpu' : id.startsWith('pin:') ? 'pin' : id.startsWith('part:') ? 'part' : 'blk');
    const used = rows.map((r, i) => (r.length ? i : -1)).filter((i) => i >= 0), gap = 38, ys = {};
    let y = 14;
    used.forEach((ri) => { const h = Math.max(...rows[ri].map((n) => sizes[kind(n.id)][1])); ys[ri] = [y, h]; y += h + gap; });
    const H = y - gap + 14;
    rows.forEach((r, ri) => {
      if (!r.length) return;
      const kw = Math.max(...r.map((n) => sizes[kind(n.id)][0])), n = r.length, sp = Math.min(kw + 12, (W - 16) / n);
      r.forEach((nd, i) => { const [w, h] = sizes[kind(nd.id)]; nd.w = Math.min(w, sp - 8); nd.h = h; nd.x = W / 2 + (i - (n - 1) / 2) * sp; nd.y = ys[ri][0] + (ys[ri][1] - h) / 2; });
    });
    let out = '<svg viewBox="0 0 ' + W + ' ' + H + '" class="cm cf" role="img" aria-label="Only the blocks this build uses, in the order the data moves">' + defs() + '<g id="cv-flows">';
    const lane = lanes();
    edges.forEach((e, i) => {
      const A = nodes.get(e.a), Bn = nodes.get(e.b), down = A.row < Bn.row, ya = R(down ? A.y + A.h : A.y), yb = R(down ? Bn.y : Bn.y + Bn.h);
      const rev = edges.some((o, oi) => o.j === e.j && o.a === e.b && o.b === e.a && oi < i); if (rev) return;
      const both = edges.some((o) => o.j === e.j && o.a === e.b && o.b === e.a), m = 'url(#cvmh' + (e.j % FLOW_COLORS.length) + ')';
      const ra = [A.x - A.w / 2 + 8, A.x + A.w / 2 - 8], rb = [Bn.x - Bn.w / 2 + 8, Bn.x + Bn.w / 2 - 8], lo = Math.max(ra[0], rb[0]), hi = Math.min(ra[1], rb[1]);
      let d;
      if (lo <= hi) {   // the two boxes overlap across: one straight vertical run
        const x = lane(false, (lo + hi) / 2, lo, hi, ya, yb);
        d = 'M' + x + ' ' + ya + ' L' + x + ' ' + yb;
      } else {   // one elbow: down the sender's column, then across into the side of the receiver
        const x0 = R(A.x), cy = R(Bn.y + Bn.h / 2), side = x0 < Bn.x ? R(Bn.x - Bn.w / 2) : R(Bn.x + Bn.w / 2);
        d = 'M' + x0 + ' ' + ya + ' L' + x0 + ' ' + cy + ' L' + side + ' ' + cy;
      }
      out += '<path class="flow" data-j="' + e.j + '" d="' + d + '" stroke="' + FLOW_COLORS[e.j % FLOW_COLORS.length] + '" marker-end="' + m + '"' + (both ? ' marker-start="' + m + '"' : '') + '/>';
    });
    out += '</g>';
    nodes.forEach((n) => {
      const id = n.id, k = kind(id), js = [...n.js];
      let nm, sub;
      if (k === 'pin') { nm = id.slice(4); sub = tight ? '' : pinText(B.pins[nm]); }
      else if (k === 'part') { const p = B.parts[id.slice(5)]; nm = p.name; sub = tight ? '' : p.note; }
      else if (k === 'cpu') { nm = WB.cpu.n; sub = 'runs the program'; }
      else { nm = WB[id].n; sub = (B.tags && B.tags[id]) || WB[id].s || ''; }
      const jc = FLOW_COLORS[(js[0] || 0) % FLOW_COLORS.length], col = (k === 'blk' || k === 'cpu') ? KINDS[WB[id].k].color : '#161312';
      out += '<g class="' + (k === 'pin' ? 'pin' : k === 'part' ? 'part' : 'cblk on') + (k === 'cpu' ? ' al' : '') + '"' + (k === 'blk' || k === 'cpu' ? ' data-k="' + WB[id].k + '"' : '') + ' data-b="' + id + '" data-js="' + js.join(',') + '" tabindex="0">';
      if (k === 'pin') out += '<rect class="pr" x="' + (n.x - n.w / 2) + '" y="' + n.y + '" width="' + n.w + '" height="' + n.h + '" rx="4"/>';
      else if (k === 'part') out += '<rect class="pt" x="' + (n.x - n.w / 2) + '" y="' + n.y + '" width="' + n.w + '" height="' + n.h + '" rx="5"/>';
      else out += '<rect class="r" x="' + (n.x - n.w / 2) + '" y="' + n.y + '" width="' + n.w + '" height="' + n.h + '" rx="5" style="--d:' + col + ';--j:' + jc + '"/>';
      const ty = n.y + (sub ? n.h / 2 - 3 : n.h / 2 + 5);
      out += T(n.x, ty, nm, 'bn' + (k === 'part' ? ' pn' : ''), tight && nm.length > 9 ? 'style="font-size:11px"' : '') + (sub ? T(n.x, ty + 15, sub, 'bt' + (k === 'part' ? ' pn' : '')) : '');
      if (k === 'blk' || k === 'cpu') js.slice(0, 4).forEach((ji, q) => { out += '<circle cx="' + (n.x + n.w / 2 - 8 - q * 11) + '" cy="' + (n.y + 8) + '" r="4.5" fill="' + FLOW_COLORS[ji % FLOW_COLORS.length] + '" stroke="#fff" stroke-width="1.5"/>'; });
      out += '</g>';
    });
    host.innerHTML = out + '</svg>';
  }

  // ---- the step list and the highlighting
  function rowName(B, id) {
    if (id.startsWith('pin:')) return id.slice(4);
    if (id.startsWith('part:')) return B.parts[id.slice(5)].name;
    return id === 'cpu' ? 'CPU' : WB[id].n;
  }
  function renderList(B) {
    const el = $('#cv-list');
    let h = '';
    B.jobs.forEach((j) => {
      h += '<div class="jg" data-j="' + j.i + '"><div class="jh" style="color:' + j.c + '"><i style="background:' + j.c + '"></i>' + esc(j.name) + '</div>';
      j.rows.forEach((r) => { h += '<div class="rw" tabindex="0" data-b="' + r.block + '" data-j="' + j.i + '" style="--c:' + j.c + '"><b>' + esc(r.step) + '</b><span>' + esc(r.by) + '</span></div>'; });
      h += '</div>';
    });
    const al = (id, b, t) => '<div class="rw" tabindex="0" data-b="' + id + '" style="--c:#161312"><b>' + b + '</b><span>' + t + '</span></div>';
    h += '<div class="jg"><div class="jh" style="color:#161312"><i style="background:#161312"></i>Under everything</div>'
      + al('flash', 'Holds the program', 'Flash, ' + fmtBytes(B.flash))
      + al('sram', 'Holds the variables and stack', 'SRAM, ' + fmtBytes(B.ram))
      + al('clk', 'Runs it all from one clock', esc(WB.clk.s)) + '</div>';
    el.innerHTML = h;
    $$('.rw', el).forEach((r) => {
      const id = r.dataset.b, ji = r.dataset.j == null ? null : +r.dataset.j;
      r.addEventListener('mouseenter', () => { S.hov = { b: id, j: ji }; applySel(); });
      r.addEventListener('mouseleave', () => { S.hov = null; applySel(); });
      const choose = () => { stopAuto(); S.pin = (S.pin && S.pin.b === id && S.pin.j === ji) ? null : { b: id, j: ji }; applySel(); };
      r.addEventListener('click', choose);
      r.addEventListener('keydown', (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); choose(); } });
    });
  }
  function applySel() {
    const B = cur(), sel = S.hov || S.pin || {}, svg = $('#cv-chipmount svg');
    if (!svg) return;
    const has = !!(sel.b || sel.j != null) && !S.all, js = has ? jobSet(B, sel) : null;   // at rest nothing is dimmed: every used block is lit
    svg.classList.toggle('has-sel', has);
    $$('.cblk,.pin,.part', svg).forEach((g) => {
      const gj = (g.dataset.js || '').split(',').filter(Boolean).map(Number);
      g.classList.toggle('dim', !(!has || g.dataset.b === sel.b || (js && gj.some((x) => js.has(x)))));
      g.classList.toggle('hit', !!((has || S.all) && sel.b && g.dataset.b === sel.b));
    });
    $$('.flow', svg).forEach((f) => { f.classList.toggle('dim', has && (f.dataset.j == null || !js || !js.has(+f.dataset.j))); });
    $$('#cv-list .rw').forEach((r) => {
      const rj = r.dataset.j == null ? null : +r.dataset.j, hi = (has || S.all) && sel.b && r.dataset.b === sel.b && (sel.j == null || sel.j === rj);
      r.classList.toggle('hi', !!hi); r.classList.toggle('dim', has && !hi && !(js && rj != null && js.has(rj)));
    });
    $$('#cv-list .jh').forEach((r) => { const j = +r.parentNode.dataset.j; r.classList.toggle('dim', !!(has && js && !js.has(j))); });
    let now = 'Tap a block or a step to follow one job.';
    if (sel.b) {
      const id = sel.b, jb = sel.j != null ? B.jobs[sel.j] : B.jobs.find((j) => j.rows.some((r) => r.block === id));
      const row = jb && jb.rows.find((r) => r.block === id), nk = B.near && B.near[id] ? id : id === 'i2c' ? 'i2c1' : id, lr = $('#cv-list .rw[data-b="' + id + '"]');
      if (row) now = '<b>' + esc(row.step) + '.</b> ' + esc(row.by) + '.';
      else if (['flash', 'sram', 'clk'].includes(id) && lr) now = '<b>' + esc($('b', lr).textContent) + '.</b> ' + esc($('span', lr).textContent) + '.';
      else if (id.startsWith('part:')) now = '<b>' + esc(rowName(B, id)) + ':</b> ' + esc(B.parts[id.slice(5)].note) + '.';
      else if (id.startsWith('pin:')) now = '<b>' + esc(rowName(B, id)) + ':</b> ' + esc(pinText(B.pins[id.slice(4)])) + '.';
      else now = '<b>' + esc(rowName(B, id)) + ':</b> ' + esc(WB[id].note || 'not used by this build') + '.' + (B.near && B.near[nk] ? ' ' + esc(B.near[nk]) : '');
    }
    const nowEl = $('#cv-now'); if (nowEl) nowEl.innerHTML = now;
  }
  function renderChip() {
    const B = cur(), host = $('#cv-chipmount');
    const focus = modeNow() === 'focus';
    if (focus) drawFocus(B, host, Math.max(300, Math.round(host.parentNode.clientWidth || 360))); else drawMap(B, host);
    host.parentNode.classList.toggle('focus', focus);
    $$('.cv-mode button').forEach((b) => b.setAttribute('aria-pressed', b.dataset.mode === modeNow() ? 'true' : 'false'));
    const svg = $('svg', host); svg.classList.add('mode-domain');
    $$('.cblk,.pin,.part', svg).forEach((g) => {
      const id = g.dataset.b;
      g.addEventListener('mouseenter', () => { S.hov = { b: id }; applySel(); });
      g.addEventListener('mouseleave', () => { S.hov = null; applySel(); });
      g.addEventListener('click', (e) => { e.stopPropagation(); stopAuto(); S.all = false; S.pin = (S.pin && S.pin.b === id) ? null : { b: id }; applySel(); });
      g.addEventListener('keydown', (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); g.dispatchEvent(new Event('click')); } });
    });
    svg.addEventListener('click', () => { stopAuto(); S.pin = null; applySel(); });
    renderList(B); drawBar(); applySel();
    const fr = host.parentNode;
    if (fr && fr.classList.contains('cv-frame')) fr.scrollLeft = Math.max(0, (fr.scrollWidth - fr.clientWidth) / 2);
  }
  // ---- autoplay: on load the jobs step through on a loop (about 2.5 s each); a tap pauses and picks; Play and Pause is one small control;
  // with prefers-reduced-motion there is no autoplay, only the Play button
  const still = matchMedia('(prefers-reduced-motion: reduce)').matches;
  S.auto = !still; S.idx = -1; S.seen = true;
  const wrap = $('.cv-chipwrap'); let btn = null, bar = null;
  if (wrap && !$('.cv-playbtn')) {
    const ctl = document.createElement('div'); ctl.className = 'cv-ctl';
    ctl.innerHTML = '<button type="button" class="cv-playbtn"></button><div class="cv-bar" role="slider" tabindex="0" aria-label="Drag to a step of the tour" aria-valuemin="1"></div>';
    wrap.insertBefore(ctl, wrap.firstChild); btn = $('.cv-playbtn', ctl); bar = $('.cv-bar', ctl);
  }
  function syncBtn() { if (btn) { btn.textContent = S.auto ? 'Pause' : 'Play'; btn.setAttribute('aria-pressed', S.auto ? 'true' : 'false'); btn.setAttribute('aria-label', S.auto ? 'Pause the tour of the chip' : 'Play the tour of the chip'); } }
  function stepsOf(B) { const out = []; B.jobs.forEach((j) => j.rows.forEach((r) => out.push({ b: r.block, j: j.i }))); ['flash', 'sram', 'clk'].forEach((b) => { if ($('#cv-list .rw[data-b="' + b + '"]')) out.push({ b }); }); return out; }
  function drawBar() {
    if (!bar || !cur()) return; const n = stepsOf(cur()).length;
    bar.innerHTML = Array.from({ length: n }, (_, i) => '<i data-i="' + i + '"></i>').join(''); bar.setAttribute('aria-valuemax', n); markBar();
  }
  function markBar() { if (!bar) return; [...bar.children].forEach((c, i) => { c.classList.toggle('now', i === S.idx); c.classList.toggle('done', i < S.idx); }); bar.setAttribute('aria-valuenow', Math.max(1, S.idx + 1)); }
  function goStep(i, rest) {   // jump to a step; at rest (paused, or at the end) every block the build uses is lit and the step's block is red
    const st = stepsOf(cur()); if (!st.length) return; S.idx = Math.max(0, Math.min(st.length - 1, i)); S.pin = st[S.idx]; S.all = !!rest; applySel(); markBar();
  }   // then the memory and clock rows, so Flash and SRAM light on their own steps
  function tick() {
    if (!S.auto || !S.seen || document.hidden || !cur()) return;
    const st = stepsOf(cur()); if (!st.length) return;
    if (S.idx === st.length - 1) { S.idx = st.length; S.pin = null; S.all = true; applySel(); markBar(); return; }   // the end of the loop: every block the build uses, lit at once
    goStep(S.idx >= st.length ? 0 : S.idx + 1, false);
  }
  function stopAuto() { if (S.auto) { S.auto = false; syncBtn(); } S.all = false; }
  if (btn) btn.addEventListener('click', () => { S.auto = !S.auto; if (S.auto) { S.all = false; tick(); } else if (S.pin) { S.all = true; applySel(); } syncBtn(); });
  if (bar) {
    const at = (e) => { const r = bar.getBoundingClientRect(), n = bar.children.length; return Math.max(0, Math.min(n - 1, Math.floor((e.clientX - r.left) / r.width * n))); };
    let down = false;
    bar.addEventListener('pointerdown', (e) => { down = true; bar.setPointerCapture(e.pointerId); S.auto = false; syncBtn(); goStep(at(e), true); });
    bar.addEventListener('pointermove', (e) => { if (down) goStep(at(e), true); });
    bar.addEventListener('pointerup', () => { down = false; });
    bar.addEventListener('keydown', (e) => { const k = e.key; if (k === 'ArrowRight' || k === 'ArrowLeft' || k === 'Home' || k === 'End') { e.preventDefault(); S.auto = false; syncBtn(); const n = stepsOf(cur()).length; goStep(k === 'Home' ? 0 : k === 'End' ? n - 1 : S.idx + (k === 'ArrowRight' ? 1 : -1), true); } });
  }
  let ticker = setInterval(tick, 2500);
  if (typeof IntersectionObserver !== 'undefined' && wrap) new IntersectionObserver((es) => { S.seen = es.some((e) => e.isIntersecting); }).observe(wrap);
  function pick(slug) {
    if (!builds[slug]) return false;
    S.cur = slug; S.pin = null; S.hov = null; S.idx = -1; S.all = false;
    $$('.cv-card').forEach((c) => { const on = c.dataset.slug === slug; c.classList.toggle('sel', on); c.setAttribute('aria-pressed', on ? 'true' : 'false'); });
    $('#cv-ptitle').textContent = builds[slug].title;
    $('#cv-wiring').innerHTML = '<b>Wiring.</b> ' + esc(builds[slug].wiring);
    renderChip(); return true;
  }
  $$('.cv-card').forEach((c) => c.addEventListener('click', () => { pick(c.dataset.slug); $('#inside').scrollIntoView({ behavior: 'smooth', block: 'start' }); }));
  $$('.cv-mode button').forEach((b) => b.addEventListener('click', () => { S.mode = b.dataset.mode; renderChip(); }));
  let rz; addEventListener('resize', () => { clearTimeout(rz); rz = setTimeout(renderChip, 150); });
  const q = new URLSearchParams(location.search).get('pick');
  pick(q && builds[q] ? q : Object.keys(builds)[0]);
  syncBtn(); if (S.auto) tick();
  return { pick };
}
