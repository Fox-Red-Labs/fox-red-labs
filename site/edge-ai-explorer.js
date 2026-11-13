// "Turn the knobs" on site/edge-ai.html: model size, where it runs, chip and clock over MEASURED points only.
// The points, their text and every number are written by tools/edge_ai_page.py (knob_data) from data/results rows;
// this file only picks one and shows it. A combination with no row says "not measured" and shows no number.
// Without JavaScript the controls stay hidden and the table of every point is the page.
(function () {
  'use strict';
  var el = document.getElementById('kn-data');
  var box = document.getElementById('kn');
  if (!el || !box) return;
  var D = JSON.parse(el.textContent);
  var $ = function (id) { return document.getElementById(id); };
  function esc(s) { return String(s).replace(/[&<>"]/g, function (c) { return {'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[c]; }); }
  var RUN = {}; D.runs.forEach(function (r) { RUN[r.id] = r.label; });
  var NAME = {}; D.boards.forEach(function (b) { NAME[b.id] = b.name; });
  var S = {mi: 2, run: 'npu', b: 'mspm0g5187', opt: 'o3', mhz: null};
  if (!NAME[S.b]) S.b = D.boards[0].id;

  function pts(f) { return D.pts.filter(f); }
  function optOf(run) { return run === 'c' ? S.opt : ''; }
  function clocks() {
    var seen = {};
    pts(function (p) { return p.b === S.b && p.run === S.run && p.opt === optOf(S.run); }).forEach(function (p) { seen[p.mhz] = 1; });
    return Object.keys(seen).map(Number).sort(function (a, b) { return b - a; });
  }
  function pick() {
    var m = D.models[S.mi].id, o = optOf(S.run);
    var c = pts(function (p) { return p.b === S.b && p.m === m && p.run === S.run && p.opt === o; });
    if (!c.length) return null;
    var at = c.filter(function (p) { return p.mhz === S.mhz; });
    return at.length ? at[0] : null;   // this model was not run at the chosen clock: say so, never borrow another clock's number
  }
  function seg(id, items, cur, name) {
    $(id).innerHTML = items.map(function (it) {
      return '<label' + (it.na ? ' class="na" title="No row for this chip"' : '') + '><input type="radio" name="' + name + '" value="' + esc(it.v) + '"' + (String(it.v) === String(cur) ? ' checked' : '') + '><span>' + esc(it.t) + '</span></label>';
    }).join('');
  }
  function kv(k, v, missing) { return '<tr><th>' + esc(k) + '</th><td>' + (v == null ? '<span class="dim">' + esc(missing) + '</span>' : esc(v)) + '</td></tr>'; }

  function draw() {
    // where it runs: dim the builds this chip has no row for, at any size
    seg('kn-run', D.runs.map(function (r) { return {v: r.id, t: r.label, na: !pts(function (p) { return p.b === S.b && p.run === r.id; }).length}; }), S.run, 'kn-run');
    var hasOs = pts(function (p) { return p.b === S.b && p.run === 'c' && p.opt === 'os'; }).length > 0;
    if (S.run === 'c' && !hasOs) S.opt = 'o3';
    $('kn-optw').hidden = !(S.run === 'c' && hasOs);
    var opt = document.querySelector('input[name="kn-opt"][value="' + S.opt + '"]'); if (opt) opt.checked = true;
    var cl = clocks();
    if (cl.indexOf(S.mhz) < 0) S.mhz = cl.length ? cl[0] : null;
    $('kn-clkw').hidden = cl.length < 2;
    if (cl.length > 1) seg('kn-clk', cl.map(function (f) { return {v: f, t: f + ' MHz'}; }), S.mhz, 'kn-clk');
    var m = D.models[S.mi], p = pick(), out = $('kn-out');
    var what = m.label + ' model (' + m.params + ' parameters, ' + m.macs + ' MACs), ' + RUN[S.run] + (S.run === 'c' ? (S.opt === 'os' ? ' -Os' : ' -O3') : '') + ', ' + NAME[S.b] + (S.mhz ? ', ' + S.mhz + ' MHz' : '');
    if (!p) {
      var have = D.runs.filter(function (r) { return pts(function (q) { return q.b === S.b && q.m === m.id && q.run === r.id; }).length; }).map(function (r) { return r.label; });
      out.innerHTML = '<div class="kn-big no">Not measured</div><p class="kn-sub">' + esc(what) + '.</p>' +
        '<p class="kn-why">No row for this combination, so no number. ' + (have.length ? 'Measured for this model on this chip: ' + esc(have.join(', ')) + '.' : 'This chip has no row for this model size.') + '</p>';
      return;
    }
    out.innerHTML = '<div class="kn-big">' + esc(p.t) + ' ms</div>' +
      '<p class="kn-sub">' + esc(what) + '. ' + esc(p.cyc) + ' cycles, ' + esc(p.cpm) + ' per MAC.</p>' +
      '<table class="kn-kv">' + kv('Flash', p.fl, 'not recorded') + kv('RAM', p.ram, 'not recorded') + kv('Accuracy', p.acc ? p.acc + ', int8, 4 classes' : null, 'not measured for this build') +
      kv('Energy', p.en ? p.en + ' per inference above idle' : null, 'not measured') + '</table>' +
      (p.why ? '<p class="kn-why">' + esc(p.why) + '</p>' : '') + '<p class="kn-src">Row: ' + esc(p.src) + '</p>';
  }

  $('kn-size').value = S.mi;
  $('kn-chip').value = S.b;
  $('kn-size').addEventListener('input', function () { S.mi = +this.value; draw(); });
  $('kn-chip').addEventListener('change', function () { S.b = this.value; draw(); });
  box.addEventListener('change', function (e) {
    var t = e.target;
    if (t.name === 'kn-run') S.run = t.value;
    else if (t.name === 'kn-clk') S.mhz = +t.value;
    else if (t.name === 'kn-opt') S.opt = t.value;
    else return;
    draw();
  });
  box.hidden = false;
  draw();
})();
