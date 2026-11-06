(function () {
  'use strict';
  var D = JSON.parse(document.getElementById('ex-data').textContent);
  var NS = 'http://www.w3.org/2000/svg';
  var CLS = {c: 'Plain C', nnc: 'NNC CPU', npu: 'NPU'};
  var $ = function (id) { return document.getElementById(id); };
  function esc(s) { return String(s).replace(/[&<>"]/g, function (c) { return {'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[c]; }); }
  function sig(v, unit) { var t; if (unit === 'k') t = v < 10 ? v.toFixed(1) : v.toFixed(0); else t = v < 10 ? v.toFixed(2) : v < 100 ? v.toFixed(1) : v.toFixed(0); return t.indexOf('.') >= 0 ? t.replace(/0+$/, '').replace(/\.$/, '') : t; }
  function cnt(n) { n = Math.round(n); var a = Math.abs(n); if (a < 1000) return String(n); var t = sig(a / 1e3, 'k'); if (+t < 1000) return t + 'k'; t = sig(a / 1e6, 'M'); if (+t < 1000) return t + 'M'; return sig(a / 1e9, 'G') + 'G'; }
  function kb(n) { n = +n; if (Math.abs(n) < 1000) return Math.round(n) + ' B'; var t = (n / 1024).toFixed(1).replace(/\.0$/, ''); if (+t < 1000) return t + ' kB'; return (n / 1048576).toFixed(1).replace(/\.0$/, '') + ' MB'; }
  function ms(v) { return v >= 100 ? Math.round(v).toLocaleString('en-US') : v >= 10 ? v.toFixed(1) : v >= 1 ? v.toFixed(2) : v >= 0.1 ? v.toFixed(3) : v.toFixed(4); }
  function xfmt(v) { return fam.xkind === 'kb' ? (+v.toFixed(1)) + ' kB' : cnt(v); }
  var fam = null, on = {}, clsOn = {c: true, nnc: false, npu: true}, last = null, START = location.hash, ready = false;
  // the data's revision: a short hash of the measurements this page carries, so a saved choice says which numbers it used
  var REV = (function (t) { var h = 2166136261; for (var i = 0; i < t.length; i++) { h ^= t.charCodeAt(i); h = Math.imul(h, 16777619) >>> 0; } return ('0000000' + h.toString(16)).slice(-8); })(document.getElementById('ex-data').textContent);
  var MODES = {cpunpu: {c: true, nnc: false, npu: true}, c: {c: true, nnc: false, npu: false}, nnc: {c: false, nnc: true, npu: false}, npu: {c: false, nnc: false, npu: true}, all: {c: true, nnc: true, npu: true}};
  D.default_on.forEach(function (b) { on[b] = true; });

  function boardOf(id) { return D.boards[id]; }
  function sPts(s) {
    var out = [];
    fam.models.forEach(function (m) {
      var p = s.pts[m.id];
      if (p) out.push({m: m, x: p.x != null ? p.x : m.x, macs: m.macs, ms: p.ms, clock: p.clock, cpm: m.macs ? p.ms * p.clock * 1000 / m.macs : null, flash: p.flash, ram: p.ram, p: p});
    });
    out.sort(function (a, b) { return a.x - b.x; });
    return out;
  }
  // log-log interpolation of f(x) between neighbours; returns {v, kind, a, b}
  function interp(pts, x, get, flat) {
    var q = pts.filter(function (p) { return get(p) != null && get(p) > 0; });
    if (!q.length) return null;
    for (var i = 0; i < q.length; i++) if (Math.abs(x / q[i].x - 1) < 0.01) return {v: get(q[i]), kind: 'measured', a: q[i], b: q[i]};
    if (x < q[0].x) return {v: get(q[0]) * (flat ? 1 : x / q[0].x), kind: 'extrap', a: q[0], b: q[0]};
    if (x > q[q.length - 1].x) { var l = q[q.length - 1]; return {v: get(l) * (flat ? 1 : x / l.x), kind: 'extrap', a: l, b: l}; }
    for (var j = 0; j < q.length - 1; j++) if (x >= q[j].x && x <= q[j + 1].x) {
      var t = (Math.log(x) - Math.log(q[j].x)) / (Math.log(q[j + 1].x) - Math.log(q[j].x));
      return {v: Math.exp(Math.log(get(q[j])) * (1 - t) + Math.log(get(q[j + 1])) * t), kind: 'interp', a: q[j], b: q[j + 1]};
    }
    return null;
  }
  function macsAt(x) {
    var q = fam.models.slice().sort(function (a, b) { return a.x - b.x; });
    var r = interp(q, x, function (m) { return m.macs; });
    return r ? r.v : x;
  }
  function xFromMacs(mac) {
    var lo = Math.log(fam.models[0].x / 1000), hi = Math.log(fam.models[fam.models.length - 1].x * 1000);
    for (var i = 0; i < 60; i++) { var mid = (lo + hi) / 2; if (macsAt(Math.exp(mid)) < mac) lo = mid; else hi = mid; }
    return Math.exp((lo + hi) / 2);
  }
  // time estimate for a series at size x: {ms, lo, hi, kind, note}
  function est(s, x) {
    var pts = sPts(s);
    if (!pts.length) return null;
    var mac = fam.fitted ? macsAt(x) : null;
    var r = interp(pts, x, function (p) { return p.cpm; }, true);
    if (!r) return null;
    var cl = pts[0].clock, t = r.v * mac / (cl * 1000), lo, hi, note;
    if (r.kind === 'measured') { t = r.a.ms; lo = hi = t; note = 'measured on this model'; }
    else if (r.kind === 'interp') {
      lo = Math.min(r.a.cpm, r.b.cpm) * mac / (cl * 1000); hi = Math.max(r.a.cpm, r.b.cpm) * mac / (cl * 1000);
      note = 'between our ' + xfmt(r.a.x) + ' and ' + xfmt(r.b.x) + ' models, ' + sig(r.v, 'M') + ' cycles per MAC';
    } else {
      var all = pts.map(function (p) { return p.cpm; });
      lo = Math.min.apply(null, all) * mac / (cl * 1000); hi = Math.max.apply(null, all) * mac / (cl * 1000);
      note = 'beyond our models: the ' + xfmt(r.a.x) + ' model\'s ' + sig(r.a.cpm, 'M') + ' cycles per MAC carried out; this is a guess';
    }
    return {ms: t, lo: lo, hi: hi, kind: r.kind, note: note};
  }
  function memEst(s, x, key) {
    var pts = sPts(s);
    var r = interp(pts, x, function (p) { return p[key]; });
    return r ? {v: r.v, kind: r.kind} : null;
  }

  // ---------- controls
  function visibleSeries() { return fam.series.filter(function (s) { return on[s.board] && clsOn[s.cls]; }); }
  function buildBoxes() {
    var box = $('ex-boards'); box.innerHTML = '';
    var have = {}, n = 0, k = 0; fam.series.forEach(function (s) { have[s.board] = true; });
    D.groups.forEach(function (g) {
      var ids = Object.keys(D.boards).filter(function (b) { return D.boards[b].group === g[0] && have[b]; });
      if (!ids.length) return;
      var div = document.createElement('div'); div.className = 'ex-grp';
      div.innerHTML = '<b>' + esc(g[1]) + '</b>' + ids.map(function (b) {
        n++; if (on[b]) k++;
        return '<label><input type="checkbox" data-b="' + b + '"' + (on[b] ? ' checked' : '') + '> ' + esc(D.boards[b].name) + ' <span class="dim">' + D.boards[b].clock + ' MHz</span></label>';
      }).join('');
      box.appendChild(div);
    });
    $('ex-bsum').textContent = 'Boards (' + k + ' of ' + n + ')';
  }
  function fillCalcBoards() {
    var sel = $('ex-board'), cur = sel.value;
    sel.innerHTML = '';
    var seen = {};
    fam.series.forEach(function (s) { if (!seen[s.board]) { seen[s.board] = 1; var o = document.createElement('option'); o.value = s.board; o.textContent = D.boards[s.board].name; sel.appendChild(o); } });
    if (seen[cur]) sel.value = cur; else sel.value = seen['mspm0g5187'] ? 'mspm0g5187' : fam.series[0].board;
  }
  function calcSeries() {
    var b = $('ex-board').value, cand = fam.series.filter(function (q) { return q.board === b; });
    var vis = cand.filter(function (q) { return clsOn[q.cls]; }), use = vis.length ? vis : cand, x = sizeX() || fam.models[0].x, bst = null;
    use.forEach(function (q) { var e = est(q, x); if (e && (!bst || e.ms < bst.e.ms)) bst = {s: q, e: e}; });
    return bst ? bst.s : null;
  }

  function sizeX() { var v = parseFloat($('ex-size').value); return v > 0 ? v : null; }

  // ---------- chart
  var W = 640, H = 440, ML = 58, MR = 14, MT = 14, MB = 44;
  function niceTicks(lo, hi) {
    var out = [], p = Math.floor(Math.log10(lo));
    for (var e = p; e <= Math.ceil(Math.log10(hi)); e++) [1, 2, 5].forEach(function (k) { var v = k * Math.pow(10, e); if (v >= lo * 0.999 && v <= hi * 1.001) out.push(v); });
    return out;
  }
  function marker(cls, cx, cy, color, extra) {
    if (cls === 'nnc') return '<rect x="' + (cx - 4) + '" y="' + (cy - 4) + '" width="8" height="8" fill="' + color + '"' + (extra || '') + '/>';
    if (cls === 'npu') return '<path d="M' + cx + ' ' + (cy - 5.5) + ' L' + (cx + 5) + ' ' + (cy + 4) + ' L' + (cx - 5) + ' ' + (cy + 4) + ' Z" fill="' + color + '"' + (extra || '') + '/>';
    return '<circle cx="' + cx + '" cy="' + cy + '" r="4.2" fill="' + color + '"' + (extra || '') + '/>';
  }
  function draw() {
    var vs = visibleSeries(), svg = $('ex-svg');
    W = svg.parentNode.clientWidth && svg.parentNode.clientWidth < 520 ? 400 : 640; svg.setAttribute('viewBox', '0 0 ' + W + ' ' + H);
    var xs = fam.models.map(function (m) { return m.x; });
    var xlo = Math.min.apply(null, xs) / 2, xhi = Math.max.apply(null, xs) * 3;
    var cx = fam.fitted ? sizeX() : null;
    if (cx) { xlo = Math.min(xlo, cx / 1.3); xhi = Math.max(xhi, cx * 1.3); }
    if (!fam.fitted) { xlo = Math.min.apply(null, xs) / 1.6; xhi = Math.max.apply(null, xs) * 1.6; }
    var lines = [], dots = [], ys = [];
    var N = 48;
    vs.forEach(function (s) {
      var pts = sPts(s); if (!pts.length) return;
      pts.forEach(function (p) { dots.push({s: s, p: p}); ys.push(p.ms); });
      if (fam.fitted && s.line) {
        var seg = [], first = pts[0].x, last = pts[pts.length - 1].x;
        for (var i = 0; i <= N; i++) {
          var x = Math.exp(Math.log(xlo) + (Math.log(xhi) - Math.log(xlo)) * i / N), e = est(s, x);
          if (e) { seg.push({x: x, y: e.ms, ext: x < first * 0.999 || x > last * 1.001}); ys.push(e.ms); }
        }
        pts.forEach(function (p) { seg.push({x: p.x, y: p.ms, ext: false}); });
        seg.sort(function (a, b) { return a.x - b.x; });
        lines.push({s: s, seg: seg});
      }
    });
    var dl = fam.fitted ? parseFloat($('ex-dl').value) : NaN;
    if (dl > 0) ys.push(dl);
    var ylo = ys.length ? Math.min.apply(null, ys) : 0.1, yhi = ys.length ? Math.max.apply(null, ys) : 100;
    ylo = Math.pow(10, Math.floor(Math.log10(ylo * 0.8))); yhi = Math.pow(10, Math.ceil(Math.log10(yhi * 1.2)));
    if (yhi <= ylo) yhi = ylo * 10;
    function X(v) { return ML + (Math.log(v) - Math.log(xlo)) / (Math.log(xhi) - Math.log(xlo)) * (W - ML - MR); }
    function Y(v) { return H - MB - (Math.log(v) - Math.log(ylo)) / (Math.log(yhi) - Math.log(ylo)) * (H - MT - MB); }
    var h = '';
    niceTicks(ylo, yhi).forEach(function (v) {
      var major = Math.abs(Math.log10(v) - Math.round(Math.log10(v))) < 1e-9;
      h += '<line x1="' + ML + '" x2="' + (W - MR) + '" y1="' + Y(v) + '" y2="' + Y(v) + '" stroke="' + (major ? '#CFC3B2' : '#EBE3D6') + '"/>';
      if (major) h += '<text x="' + (ML - 8) + '" y="' + (Y(v) + 4) + '" text-anchor="end" class="ax">' + (v >= 1 ? v.toLocaleString('en-US') : v) + '</text>';
    });
    niceTicks(xlo, xhi).forEach(function (v) {
      var lead = +(v / Math.pow(10, Math.floor(Math.log10(v)))).toFixed(0);
      h += '<line x1="' + X(v) + '" x2="' + X(v) + '" y1="' + MT + '" y2="' + (H - MB) + '" stroke="' + (lead === 1 ? '#DDD2C2' : '#EFE8DC') + '"/>';
      if (lead === 1 || lead === 2 || lead === 5) h += '<text x="' + X(v) + '" y="' + (H - MB + 16) + '" text-anchor="middle" class="ax">' + xfmt(v) + '</text>';
    });
    h += '<line x1="' + ML + '" x2="' + ML + '" y1="' + MT + '" y2="' + (H - MB) + '" stroke="#161312"/><line x1="' + ML + '" x2="' + (W - MR) + '" y1="' + (H - MB) + '" y2="' + (H - MB) + '" stroke="#161312"/>';
    h += '<text x="' + (ML + (W - ML - MR) / 2) + '" y="' + (H - 8) + '" text-anchor="middle" class="axt">' + esc(fam.xlabel) + ' (log scale)</text>';
    h += '<text transform="translate(14 ' + (MT + (H - MT - MB) / 2) + ') rotate(-90)" text-anchor="middle" class="axt">Time per inference, ms (log scale)</text>';
    if (dl > 0 && Y(dl) >= MT && Y(dl) <= H - MB) h += '<line x1="' + ML + '" x2="' + (W - MR) + '" y1="' + Y(dl) + '" y2="' + Y(dl) + '" stroke="#B42418" stroke-dasharray="6 4"/><text x="' + (W - MR - 4) + '" y="' + (Y(dl) - 5) + '" text-anchor="end" class="axr">deadline ' + ms(dl) + ' ms</text>';
    if (cx && X(cx) > ML && X(cx) < W - MR) h += '<line x1="' + X(cx) + '" x2="' + X(cx) + '" y1="' + MT + '" y2="' + (H - MB) + '" stroke="#6B625B" stroke-dasharray="2 4"/><text x="' + (X(cx) + 4) + '" y="' + (MT + 12) + '" class="axs">your size ' + xfmt(cx) + '</text>';
    lines.forEach(function (L) {
      var color = D.boards[L.s.board].color, solid = '', ext = '', cur = [], curExt = null;
      function flush() { if (cur.length > 1) { var d = 'M' + cur.map(function (p) { return X(p.x).toFixed(1) + ' ' + Y(p.y).toFixed(1); }).join(' L'); if (curExt) ext += '<path d="' + d + '" fill="none" stroke="' + color + '" stroke-width="1.5" stroke-dasharray="4 3" opacity=".75"/>'; else solid += '<path d="' + d + '" fill="none" stroke="' + color + '" stroke-width="1.8"/>'; } }
      L.seg.forEach(function (p, i) {
        if (curExt === null) curExt = p.ext;
        if (p.ext !== curExt) { var prev = cur[cur.length - 1]; flush(); cur = prev ? [prev] : []; curExt = p.ext; }
        cur.push(p);
      });
      flush(); h += solid + ext;
    });
    dots.forEach(function (d) {
      var p = d.p, s = d.s, color = D.boards[s.board].color;
      var tip = D.boards[s.board].name + ', ' + s.build + '. ' + d.p.m.label + ': ' + ms(p.ms) + ' ms at ' + p.clock + ' MHz' + (p.flash != null ? '; flash ' + kb(p.flash) + ', RAM ' + kb(p.ram) : '') + (p.p.runs ? '; ' + p.p.runs + ' run' + (p.p.runs === '1' ? '' : 's') : '');
      h += '<g>' + marker(s.cls, X(p.x).toFixed(1), Y(p.ms).toFixed(1), color, ' stroke="#fff" stroke-width="1"') + '<title>' + esc(tip) + '</title></g>';
    });
    if (cx && fam.fitted) {
      var cs = calcSeries();
      var e = cs ? est(cs, cx) : null;
      if (e && X(cx) > ML && X(cx) < W - MR && Y(e.ms) > MT && Y(e.ms) < H - MB) h += '<circle cx="' + X(cx) + '" cy="' + Y(e.ms) + '" r="9" fill="none" stroke="#161312" stroke-width="2"/>';
    }
    svg.innerHTML = h;
    var leg = vs.map(function (s) {
      return '<span class="lg"><svg width="16" height="14" viewBox="0 0 16 14" aria-hidden="true">' + marker(s.cls, 8, 7, D.boards[s.board].color) + '</svg> ' + esc(D.boards[s.board].name) + ', ' + CLS[s.cls] + '</span>';
    }).join('');
    $('ex-legend').innerHTML = leg || '<span class="dim">Tick at least one board and one build.</span>';
    // table of measured points
    var hd = '<tr><th>Board and build</th>' + fam.models.map(function (m) { return '<th>' + esc(m.label) + '</th>'; }).join('') + '</tr>';
    var body = vs.map(function (s) {
      return '<tr><td>' + esc(D.boards[s.board].name) + '<span class="cond">' + esc(CLS[s.cls] + ', ' + s.build + ', ' + s.clock + ' MHz') + '</span></td>' + fam.models.map(function (m) {
        var p = s.pts[m.id]; if (!p) return '<td class="dim">none</td>';
        return '<td><b>' + ms(p.ms) + ' ms</b>' + (p.flash != null ? '<span class="cond">' + kb(p.flash) + ' flash, ' + kb(p.ram) + ' RAM</span>' : '') + '</td>';
      }).join('') + '</tr>';
    }).join('');
    $('ex-table').innerHTML = '<thead>' + hd + '</thead><tbody>' + body + '</tbody>';
  }

  // ---------- calculator
  function calc() {
    var box = $('ex-calc-body'), off = $('ex-calc-off');
    if (!fam.fitted) { box.hidden = true; off.hidden = false; return; }
    box.hidden = false; off.hidden = true;
    var x = sizeX(), dl = parseFloat($('ex-dl').value), out = $('ex-out');
    if (!x || !(dl > 0)) { out.innerHTML = '<p class="ex-note">Enter a size and a deadline above zero.</p>'; return; }
    var b = $('ex-board').value, s = calcSeries();
    if (!s) { out.innerHTML = ''; return; }
    var bd = D.boards[b], e = est(s, x), fl = memEst(s, x, 'flash'), rm = memEst(s, x, 'ram');
    var verdict;
    if (e.ms <= dl / 2) verdict = '<b>Yes</b>, with room (under half of ' + ms(dl) + ' ms)';
    else if (e.ms <= dl) verdict = '<b>Tight</b>: under ' + ms(dl) + ' ms, over half of it';
    else verdict = '<b class="bad">No</b>: misses ' + ms(dl) + ' ms';
    if (e.kind !== 'measured' && e.hi > dl && e.lo <= dl) verdict += '; the spread crosses the deadline';
    var spread = e.kind === 'measured' ? ', measured' : ', estimate ' + ms(e.lo) + ' to ' + ms(e.hi) + ' ms';
    var rows = '<tr><th>Time</th><td><b class="big">' + ms(e.ms) + ' ms</b> <span class="dim">' + esc(CLS[s.cls]) + spread + '</span></td></tr>' +
      '<tr><th>Meets deadline</th><td>' + verdict + '</td></tr>';
    if (fl && rm) {
      var F = bd.flash_kb * 1024, R = bd.ram_kb * 1024, fbad = fl.v > F, rbad = rm.v > R;
      var why = fbad && rbad ? 'flash and RAM' : fbad ? 'flash' : rbad ? 'RAM' : '';
      rows += '<tr><th>Flash</th><td><b>' + kb(fl.v) + '</b> of ' + kb(F) + ' (' + Math.round(fl.v / F * 100) + '%)</td></tr>' +
        '<tr><th>RAM</th><td><b>' + kb(rm.v) + '</b> of ' + kb(R) + ' (' + Math.round(rm.v / R * 100) + '%)</td></tr>' +
        '<tr><th>Fits</th><td>' + (why ? '<b class="bad">No: ' + why + '</b>' : '<b>Yes</b>, the model alone') + '</td></tr>';
    } else rows += '<tr><th>Flash, RAM</th><td class="dim">Not recorded for this build.</td></tr>';
    var scale = [0.5, 1, 2, 4].map(function (k) {
      var xx = x * k, ee = est(s, xx), ff = memEst(s, xx, 'flash'), rr = memEst(s, xx, 'ram');
      return '<td' + (k === 1 ? ' class="now"' : '') + '><span class="cond">' + (k === 1 ? 'this size' : k + 'x') + '</span><b>' + xfmt(xx) + '</b><br>' + ms(ee.ms) + ' ms' + (ff ? '<span class="cond">' + kb(ff.v) + ' / ' + kb(rr ? rr.v : 0) + '</span>' : '') + '</td>';
    }).join('');
    last = {board: b, s: s, e: e, x: x, dl: dl, fl: fl, rm: rm, bd: bd, verdict: verdict.replace(/<[^>]+>/g, '')};
    out.innerHTML = '<table class="ex-kv"><tbody>' + rows + '</tbody></table>' +
      '<details class="ex-fold"><summary>If the model grows or shrinks, and how sure this is</summary><table class="ex-scale"><tbody><tr>' + scale + '</tr></tbody></table><p class="ex-note">' + esc(e.note) + '. Build used: ' + esc(s.build) + ' at ' + s.clock + ' MHz.</p></details>';
  }

  // ---------- wiring
  function setMode(m) { clsOn = JSON.parse(JSON.stringify(MODES[m])); $('ex-mode').value = m; }
  function setFamily(i) {
    fam = D.families[i];
    setMode($('ex-mode').value || 'cpunpu');
    var seen = fam.series.some(function (s) { return on[s.board] && clsOn[s.cls]; });
    if (!seen) { fam.series.forEach(function (s) { on[s.board] = true; }); setMode('all'); }
    buildBoxes(); fillCalcBoards();
    if (fam.fitted) $('ex-size').value = fam.models[Math.min(2, fam.models.length - 1)].x;
    $('ex-fam-note').textContent = fam.note;
    all();
  }
  function all() { draw(); calc(); toHash(); }

  // ---------- keep the choice: a link that reproduces it, and a file an engineer can file with a design review
  // (review B08, 2026-10-15). The link carries the data revision; a link made on other data says so.
  function toHash() {
    if (!ready || !history.replaceState) return;   // only once the visitor has changed something: a plain visit keeps its own #anchor
    var q = new URLSearchParams();
    q.set('fam', fam.id); q.set('mode', $('ex-mode').value); q.set('board', $('ex-board').value);
    q.set('on', Object.keys(on).filter(function (b) { return on[b]; }).join('.'));
    if (fam.fitted) { q.set('size', $('ex-size').value); q.set('dl', $('ex-dl').value); }
    q.set('rev', REV);
    history.replaceState(null, '', location.pathname + location.search + '#ex&' + q.toString());
  }
  function fromHash() {
    if (START.indexOf('#ex&') !== 0) return;
    var q = new URLSearchParams(START.slice(4)), note = [];
    var i = D.families.findIndex(function (f) { return f.id === q.get('fam'); });
    if (i < 0) { if (q.get('fam')) note.push('the link names a model family this page no longer has'); return say(note); }
    $('ex-fam').value = String(i); setFamily(i);
    if (MODES[q.get('mode')]) setMode(q.get('mode'));
    if (q.get('on') !== null) { on = {}; q.get('on').split('.').forEach(function (b) { if (D.boards[b]) on[b] = true; else if (b) note.push('board ' + b + ' is not in this data'); }); }
    if (fam.fitted) { if (+q.get('size') > 0) $('ex-size').value = q.get('size'); if (+q.get('dl') > 0) $('ex-dl').value = q.get('dl'); }
    if (q.get('board') && [].some.call($('ex-board').options, function (o) { return o.value === q.get('board'); })) $('ex-board').value = q.get('board');
    if (q.get('rev') && q.get('rev') !== REV) note.push('this link was made with data revision ' + q.get('rev') + '; the numbers shown are revision ' + REV + ' and may differ');
    buildBoxes(); all(); say(note);
  }
  function say(note) { $('ex-saved').textContent = note.length ? 'Note: ' + note.join('; ') + '.' : ''; }
  function save() {
    if (!last) { $('ex-saved').textContent = 'Nothing to save: this family has no estimate.'; return; }
    var L = last, est = L.e, kind = est.kind === 'measured' ? 'measured' : est.kind === 'interp' ? 'estimate, between two measured sizes' : 'estimate, outside the measured sizes';
    var out = {
      what: 'Fox Red Labs edge AI explorer: one model size on one board, against one deadline',
      page: location.href, data_revision: REV, saved: new Date().toISOString().slice(0, 10),
      family: fam.label, model_size: {value: L.x, unit: fam.xlabel || 'parameters'}, deadline_ms: L.dl,
      board: {id: L.board, name: L.bd.name, flash_kb: L.bd.flash_kb, ram_kb: L.bd.ram_kb},
      build: {class: CLS[L.s.cls], build: L.s.build, clock_mhz: L.s.clock},
      time_ms: {value: est.ms, low: est.lo, high: est.hi, kind: kind, how: est.note},
      meets_deadline: L.verdict,
      memory: L.fl && L.rm ? {flash_bytes: L.fl.v, ram_bytes: L.rm.v, kind: 'the model alone, from measured builds of the family; not a linked application with your code'} : 'not recorded for this build',
      limits: ['Time is the model call on the chip, measured or estimated from measured cycles per multiply-add; it says nothing about accuracy on your data or live sensors.',
        'Builds ticked on the page decide which series is used; the fastest ticked build for the board is taken.',
        'Compiler and SDK versions are in the run records cited on the page, not in this file.']
    };
    var a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([JSON.stringify(out, null, 1)], {type: 'application/json'}));
    a.download = 'edge-ai-choice-' + L.board + '.json'; document.body.appendChild(a); a.click(); a.remove();
    $('ex-saved').textContent = 'Saved ' + a.download + ' (data revision ' + REV + ').';
  }
  $('ex-save').addEventListener('click', save);
  $('ex-link').addEventListener('click', function () {
    toHash(); var t = location.href, btn = this;
    var done = function () { btn.textContent = 'Copied'; setTimeout(function () { btn.textContent = 'Copy a link to this choice'; }, 1500); };
    if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(t).then(done, function () { $('ex-saved').textContent = t; }); else $('ex-saved').textContent = t;
  });
  $('ex-fam').innerHTML = D.families.map(function (f, i) { return '<option value="' + i + '">' + esc(f.label) + '</option>'; }).join('');
  $('ex-fam').addEventListener('change', function () { setFamily(+this.value); });
  $('ex-mode').addEventListener('change', function () { setMode(this.value); all(); });
  $('ex-boards').addEventListener('change', function (ev) { var b = ev.target.getAttribute('data-b'); if (b) { on[b] = ev.target.checked; buildBoxes(); all(); } });
  $('ex-all').addEventListener('click', function () { fam.series.forEach(function (s) { on[s.board] = true; }); buildBoxes(); all(); });
  $('ex-none').addEventListener('click', function () { fam.series.forEach(function (s) { on[s.board] = false; }); buildBoxes(); all(); });
  $('ex-size').addEventListener('input', all);
  $('ex-dl').addEventListener('input', all);
  $('ex-board').addEventListener('change', function () { on[this.value] = true; buildBoxes(); all(); });
  window.addEventListener('resize', function () { var n = $('ex-svg').parentNode.clientWidth < 520 ? 400 : 640; if (n !== W) draw(); });
  setFamily(0);
  fromHash();
  ready = true;
  window.__ex = {setFamily: setFamily};
})();
