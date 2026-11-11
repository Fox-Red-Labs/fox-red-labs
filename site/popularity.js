/* Build popularity: view counts, Digital Lab flashes and hearts (docs/popularity.md).
   Counts only, no cookies and no identifiers sent. Every failure is silent: with the counter unreachable the page shows nothing extra. */
(function () {
  'use strict';
  var API = window.FRL_POP_API || 'https://counts.foxredlabs.dev';   // the one place the worker's address lives
  var LIKED_KEY = 'frl-liked';
  var counts = null, week = null;

  function liked() { try { return JSON.parse(localStorage.getItem(LIKED_KEY) || '[]'); } catch (e) { return []; } }
  function saveLiked(l) { try { localStorage.setItem(LIKED_KEY, JSON.stringify(l)); } catch (e) { /* private window */ } }
  function call(method, path) {
    return fetch(API + path, { method: method, mode: 'cors', credentials: 'omit', cache: 'no-store' })
      .then(function (r) { if (!r.ok) throw new Error(r.status); return r.json(); });
  }
  function robot() { return navigator.webdriver && !window.FRL_POP_COUNT_ROBOTS; }   // our own checks do not count

  function hit(kind, slug) {
    if (!slug || robot()) return;
    call('POST', '/hit/' + kind + '/' + encodeURIComponent(slug)).catch(function () {});
  }
  window.frlPop = { flash: function (slug) { hit('flashes', slug); } };

  // a view: once per page load, only when the page is really shown (not prefetched, not in a background tab)
  function view() {
    var h = document.querySelector('[data-heart-page]');
    if (!h) return;
    var slug = h.getAttribute('data-heart');
    function go() {
      if (document.visibilityState !== 'visible') { document.addEventListener('visibilitychange', function f() { if (document.visibilityState === 'visible') { document.removeEventListener('visibilitychange', f); hit('views', slug); } }); return; }
      hit('views', slug);
    }
    if (document.prerendering) document.addEventListener('prerenderingchange', go, { once: true }); else go();
  }

  function paintHearts() {
    var mine = liked();
    document.querySelectorAll('[data-heart]').forEach(function (b) {
      var slug = b.getAttribute('data-heart'), c = counts && counts[slug], on = mine.indexOf(slug) >= 0;
      b.hidden = !counts;
      b.setAttribute('aria-pressed', on ? 'true' : 'false');
      b.classList.toggle('on', on);
      var n = b.querySelector('.hn'), likes = c ? c.likes : 0;
      if (n) { n.textContent = likes > 0 ? String(likes) : ''; }
      b.setAttribute('aria-label', (on ? 'Remove your heart from this build' : 'Heart this build') + (likes > 0 ? ' (' + likes + ')' : ''));
    });
  }

  function toggle(b) {
    var slug = b.getAttribute('data-heart'), mine = liked(), was = mine.indexOf(slug) >= 0, want = !was;
    var c = counts && counts[slug]; if (!c) return;
    var before = c.likes;
    c.likes = Math.max(0, c.likes + (want ? 1 : -1));
    saveLiked(want ? mine.concat(slug) : mine.filter(function (s) { return s !== slug; }));
    paintHearts();
    call('POST', '/like/' + encodeURIComponent(slug) + '?set=' + (want ? '1' : '0')).then(function (r) {
      c.likes = r.likes;
      var m = liked().filter(function (s) { return s !== slug; }); saveLiked(r.liked ? m.concat(slug) : m);
      paintHearts(); sortNow();
    }).catch(function () {
      c.likes = before;
      var m = liked().filter(function (s) { return s !== slug; }); saveLiked(was ? m.concat(slug) : m);
      paintHearts();
    });
  }

  // the sort control on the builds page
  var cards = [], sel = null;
  var SCORE = {
    views: function (s) { return counts[s].views; },
    flashes: function (s) { return counts[s].flashes; },
    likes: function (s) { return counts[s].likes; },
    trending: function (s) { return week && week[s] ? week[s].views + 3 * week[s].flashes : 0; }
  };
  function sortNow() {
    if (!sel || !counts) return;
    var mode = sel.value;
    document.querySelectorAll('.pgrid').forEach(function (g) {
      var cs = [].slice.call(g.querySelectorAll('.pc[data-slug]'));
      cs.sort(function (a, b) {
        var ia = +a.dataset.i, ib = +b.dataset.i;
        if (mode === 'new') return ia - ib;
        var d = SCORE[mode](b.dataset.slug) - SCORE[mode](a.dataset.slug);
        return d || ia - ib;
      });
      cs.forEach(function (c) { g.appendChild(c); });
    });
  }
  function initSort() {
    var wrap = document.getElementById('popsort'); sel = document.getElementById('popsort-sel');
    if (!wrap || !sel) return;
    cards = [].slice.call(document.querySelectorAll('.pc[data-slug]')); cards.forEach(function (c, i) { c.dataset.i = i; });
    var any = Object.keys(counts).some(function (s) { var c = counts[s]; return c.views > 0 || c.flashes > 0 || c.likes > 0; });
    wrap.hidden = !any;   // day one shows nothing: no zeros, no sample numbers
    sel.addEventListener('change', function () {
      if (sel.value === 'trending' && !week) {
        call('GET', '/counts?window=7').then(function (r) { week = r.counts; sortNow(); }).catch(function () { sel.value = 'new'; sortNow(); });
      } else sortNow();
    });
  }

  function start() {
    view();
    var hearts = document.querySelectorAll('[data-heart]'), sortbox = document.getElementById('popsort');
    if (!hearts.length && !sortbox) return;
    call('GET', '/counts').then(function (r) {
      counts = r.counts; paintHearts(); initSort();
    }).catch(function () { /* worker unreachable: nothing extra appears */ });
    document.addEventListener('click', function (e) {
      var b = e.target.closest && e.target.closest('[data-heart]');
      if (b && !b.hidden) { e.preventDefault(); toggle(b); }
    });
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start); else start();
})();
