/* Motion kit (see motion.css). Marks what should reveal as it scrolls in; reveals it once.
   Top-level blocks of a page and every chart get the treatment; a page can opt a block out with data-m="off". */
(function () {
  var root = document.documentElement;
  root.classList.add('js');
  if (!('IntersectionObserver' in window)) { root.classList.remove('js'); return; }
  var main = document.querySelector('main .wrap') || document.querySelector('main');
  if (!main) return;
  var blocks = Array.prototype.slice.call(main.children).filter(function (e) {
    return !/^(STYLE|SCRIPT|H1|P)$/.test(e.tagName) && e.getAttribute('data-m') !== 'off' && !e.classList.contains('crumb');
  });
  blocks.forEach(function (e) { e.classList.add('m'); });
  var charts = main.querySelectorAll('.rk, .bars, .ladder, .lad');
  Array.prototype.forEach.call(charts, function (c) {
    c.classList.add('m-bars');
    var rows = c.querySelectorAll('.rk-rows > *');
    for (var i = 0; i < rows.length; i++) rows[i].style.setProperty('--i', Math.floor(i / 3));
  });
  var tiles = main.querySelectorAll('.rkt');
  Array.prototype.forEach.call(tiles, function (t) { t.classList.add('m'); });
  var io = new IntersectionObserver(function (entries) {
    entries.forEach(function (en) {
      if (en.isIntersecting) { en.target.classList.add('m-in'); io.unobserve(en.target); }
    });
  }, { rootMargin: '0px 0px -8% 0px', threshold: 0.05 });
  Array.prototype.forEach.call(main.querySelectorAll('.m, .m-bars'), function (e) { io.observe(e); });
  // anything already in view on load shows at once, without the slide
  setTimeout(function () {
    Array.prototype.forEach.call(main.querySelectorAll('.m, .m-bars'), function (e) {
      var r = e.getBoundingClientRect();
      if (r.top < innerHeight && r.bottom > 0) e.classList.add('m-in');
    });
  }, 0);
})();

/* Home page: reveal once, the portability rail, count-ups. Only runs where tools/render_home.py wrote .hm-home, and only when
   motion is allowed; otherwise the page stays as rendered (every chip lit, every number final). */
(function () {
  var home = document.querySelector('.hm-home');
  if (!home || !('IntersectionObserver' in window)) return;
  var rm = window.matchMedia && matchMedia('(prefers-reduced-motion: reduce)');
  if (rm && rm.matches) return;
  var root = document.documentElement;
  root.classList.add('hm-js');
  var rail = home.querySelector('.hm-rail svg');
  var lit = rail ? rail.querySelectorAll('[data-k]') : [];
  Array.prototype.forEach.call(lit, function (e) { e.classList.remove('lit'); });
  var prog = rail && rail.querySelector('.prog'), ball = rail && rail.querySelector('.ball');
  if (prog) prog.setAttribute('x2', rail.dataset.x0);
  function ride() {
    var x0 = +rail.dataset.x0, x1 = +rail.dataset.x1, dx = +rail.dataset.dx, t0 = performance.now(), shown = -1;
    (function f(now) {
      var p = Math.min(1, ((now || performance.now()) - t0) / 4500), x = x0 + (x1 - x0) * (1 - Math.pow(1 - p, 3));
      prog.setAttribute('x2', x); ball.setAttribute('transform', 'translate(' + (x - x0) + ' 0)');
      var k = Math.min(Math.round((x1 - x0) / dx), Math.floor((x - x0) / dx + 0.02));
      if (p >= 1) k = Math.round((x1 - x0) / dx);
      for (; shown < k; shown++) Array.prototype.forEach.call(rail.querySelectorAll('[data-k="' + (shown + 1) + '"]'), function (e) { e.classList.add('lit'); });
      if (p < 1) requestAnimationFrame(f); else prog.setAttribute('x2', 1120);
    })();
  }
  function count(el) {
    var n = +el.dataset.n, t0 = performance.now();
    el.textContent = '0';
    (function f(now) {
      var p = Math.min(1, ((now || performance.now()) - t0) / 1400);
      el.textContent = Math.round((1 - Math.pow(1 - p, 3)) * n);
      if (p < 1) requestAnimationFrame(f);
    })();
  }
  var io = new IntersectionObserver(function (es) {
    es.forEach(function (en) {
      if (!en.isIntersecting) return;
      var el = en.target;
      el.classList.add('in'); io.unobserve(el);
      if (el.id === 'hm-rail' && rail) ride();
      Array.prototype.forEach.call(el.querySelectorAll('[data-n]'), count);
    });
  }, { rootMargin: '0px 0px -8% 0px', threshold: 0.12 });
  Array.prototype.forEach.call(home.querySelectorAll('.hm-rv, .hm-stag'), function (e) { io.observe(e); });
})();
