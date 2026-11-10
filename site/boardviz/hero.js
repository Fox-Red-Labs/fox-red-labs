// The project page hero: the drawn board (the same BoardViz the board page uses) with Reset view and the numbered dots where the board has
// callouts. A page has one hero per version; only the version shown is mounted, so one WebGL context is live at a time. Data: the page's
// #hero-boards JSON ({board id: {data: the board drawing, callouts: [[part id, name]]}}). tools/render_projects.py writes the markup.
import { BoardViz, webglOk } from './boardviz.js';

const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const live = new Map();

function mount(slot, BOARDS) {
  const b = BOARDS[slot.dataset.board];
  if (!b || !webglOk()) { slot.classList.add('hm-nogl'); return; }
  const stage = slot.querySelector('.bviz'), overlay = slot.querySelector('.hm-dots'), tip = slot.querySelector('.hm-tip');
  const callouts = b.callouts || [];
  const viz = new BoardViz(stage, b.data, { azimuth: 0, elevation: 1.15, showLabels: false });
  viz.view.zoom = 0.95; viz.dirty = true;
  let sig = '', tipI = -1, raf = 0, dead = false;
  function showTip(i, x, y) {
    tipI = i;
    if (i < 0) { tip.hidden = true; return; }
    tip.innerHTML = '<b>' + (i + 1) + '</b>' + esc(callouts[i][1]); tip.hidden = false;
    const W = slot.clientWidth, w = tip.offsetWidth;
    let left = x + 16; if (left + w > W - 8) left = Math.max(8, x - 16 - w);
    tip.style.left = left + 'px'; tip.style.top = Math.max(8, y - 14) + 'px';
  }
  function layout() {
    if (!viz.size || !viz.size.w) return;
    const W = viz.size.w, H = viz.size.h, R = W >= 640 ? 11 : 12, pts = [];
    callouts.forEach(([id], i) => { const g = viz.byId[id]; if (!g) return; const p = viz.toScreen(viz.worldOf(g)); pts.push({ i, x: p.x, y: p.y }); });
    const s2 = [W, H, tipI, ...pts.map((p) => Math.round(p.x) + ',' + Math.round(p.y))].join('|');
    if (s2 === sig) return; sig = s2;
    let svg = '';
    pts.forEach((p) => { svg += '<g class="dg" data-i="' + p.i + '" role="button" tabindex="0" aria-label="' + esc(callouts[p.i][1]) + '"><circle cx="' + p.x + '" cy="' + p.y + '" r="' + (R + 8) + '" class="hit"/><circle cx="' + p.x + '" cy="' + p.y + '" r="' + R + '" class="dt' + (p.i === tipI ? ' on' : '') + '"/><text x="' + p.x + '" y="' + (p.y + 4) + '" class="dn">' + (p.i + 1) + '</text></g>'; });
    overlay.setAttribute('viewBox', '0 0 ' + W + ' ' + H); overlay.innerHTML = svg;
    if (tipI >= 0) { const p = pts.find((q) => q.i === tipI); if (p) showTip(tipI, p.x, p.y); }
  }
  function toggle(g) {
    const i = +g.dataset.i;
    if (i === tipI) { showTip(-1); sig = ''; return; }
    const c = g.querySelector('.dt'); showTip(i, +c.getAttribute('cx'), +c.getAttribute('cy')); sig = '';
  }
  overlay.addEventListener('click', (e) => { const g = e.target.closest('.dg'); if (g) toggle(g); });
  overlay.addEventListener('keydown', (e) => { if (e.key !== 'Enter' && e.key !== ' ') return; const g = e.target.closest('.dg'); if (g) { e.preventDefault(); toggle(g); } });
  slot.addEventListener('click', (e) => { if (!e.target.closest('.dg, .hm-corner')) { showTip(-1); sig = ''; } });
  slot.querySelector('[data-reset]').addEventListener('click', () => viz.resetView());
  (function go() { if (dead) return; layout(); raf = requestAnimationFrame(go); })();
  live.set(slot, () => {
    dead = true; cancelAnimationFrame(raf); viz.loop = () => {};
    try { viz.renderer.dispose(); viz.renderer.forceContextLoss(); } catch (e) { /* the context is already gone */ }
    stage.replaceChildren(); overlay.replaceChildren(); tip.hidden = true;
  });
}

export function mountHeroes() {
  const node = document.getElementById('hero-boards'); if (!node) return;
  const BOARDS = JSON.parse(node.textContent), slots = [...document.querySelectorAll('.hm[data-board]')];
  function sync() {
    slots.forEach((slot) => {
      const shown = slot.offsetParent !== null;   // an ancestor with display none (a version that is not shown) has no offsetParent
      if (shown && !live.has(slot)) mount(slot, BOARDS);
      else if (!shown && live.has(slot)) { live.get(slot)(); live.delete(slot); }
    });
  }
  window.addEventListener('frl-route', sync);
  sync();
}
