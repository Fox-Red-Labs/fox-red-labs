// The board page hero: the drawn board with numbered black dots, one tooltip at a time, and a small corner picture that swaps in the real photo.
// Tap a dot: one tooltip beside it and the dot turns red. Tap it again, or the background, to close it. No key list, no leader lines.
// The dots follow the board as it turns. Data: the board drawing (data/boardviz/<board>.json) and the callout list (data/chip-jobs/<board>.json).
import { BoardViz, webglOk } from './boardviz.js';

const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

export function mountCallouts(root, { board, callouts, photo, thumb }) {
  const $ = (s) => root.querySelector(s);
  const slot = $('#cv-slot'), stage = $('#cv-stage'), overlay = $('#cv-dots'), tip = $('#cv-tip'), swap = $('#cv-swap');
  let viz = null, sig = '', tipI = -1;

  function showTip(i, x, y) {
    tipI = i;
    if (i < 0) { tip.hidden = true; return; }
    tip.innerHTML = '<b>' + (i + 1) + '</b>' + esc(callouts[i][1]); tip.hidden = false;
    const W = slot.clientWidth, w = tip.offsetWidth;
    let left = x + 16; if (left + w > W - 8) left = Math.max(8, x - 16 - w);
    tip.style.left = left + 'px'; tip.style.top = Math.max(8, y - 14) + 'px';
  }
  function layout() {
    if (!viz || !viz.size || !viz.size.w) return;
    const W = viz.size.w, H = viz.size.h, R = W >= 640 ? 11 : 12;
    const pts = [];
    callouts.forEach(([id], i) => { const g = viz.byId[id]; if (!g) return; const p = viz.toScreen(viz.worldOf(g)); pts.push({ i, x: p.x, y: p.y }); });
    const s2 = [W, H, tipI, ...pts.map((p) => Math.round(p.x) + ',' + Math.round(p.y))].join('|');
    if (s2 === sig) return; sig = s2;
    let svg = '';
    pts.forEach((p) => {
      svg += '<g class="dg" data-i="' + p.i + '" role="button" tabindex="0" aria-label="' + esc(callouts[p.i][1]) + '"><circle cx="' + p.x + '" cy="' + p.y + '" r="' + (R + 8) + '" class="hit"/>'
        + '<circle cx="' + p.x + '" cy="' + p.y + '" r="' + R + '" class="dt' + (p.i === tipI ? ' on' : '') + '"/><text x="' + p.x + '" y="' + (p.y + 4) + '" class="dn">' + (p.i + 1) + '</text></g>';
    });
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
  slot.addEventListener('click', (e) => { if (!e.target.closest('.dg, .cv-swap, .cv-corner')) { showTip(-1); sig = ''; } });

  // the corner picture swaps the drawing and the real photo
  function swapTo(showPhoto) {
    if (!swap) return;   // a board without a bench photo has no corner picture
    slot.classList.toggle('show-photo', showPhoto); showTip(-1); sig = '';
    swap.querySelector('img').src = showPhoto ? thumb : photo;
    swap.querySelector('span').textContent = showPhoto ? 'Drawing' : 'Real board';
    swap.setAttribute('aria-label', showPhoto ? 'Show the drawing' : 'Show the real board photo');
    swap.setAttribute('aria-pressed', showPhoto ? 'true' : 'false');
  }
  if (swap) swap.addEventListener('click', () => swapTo(!slot.classList.contains('show-photo')));
  root.querySelectorAll('[data-reset]').forEach((b) => b.addEventListener('click', () => viz && viz.resetView()));

  if (!webglOk()) { stage.classList.add('bviz-nogl'); swapTo(true); return; }
  const data = JSON.parse(document.getElementById('bviz-' + board).textContent);
  viz = new BoardViz(stage, data, { azimuth: 0, elevation: 1.15, showLabels: false });
  viz.view.zoom = 0.95; viz.dirty = true;
  (function go() { layout(); requestAnimationFrame(go); })();
}
