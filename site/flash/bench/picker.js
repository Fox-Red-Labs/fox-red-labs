// The demo picker's data, as pure functions (no page, no board): every demo for one board, from the bench's own
// recipes (they run from this page) and from the project pages that ran end to end on that board (they run on your
// computer, with a guide). Nothing is invented: a row exists only where a prebuilt image or a project recipe does.
// demos.json is written by tools/webflash/build_ui.py from site/data/projects.json.

export const LEVELS = ["easy", "medium", "hard"];
const LEVEL_NAME = { easy: "Easy", medium: "Medium", hard: "Hard" };
// The level of a bench demo that no project page names, by what it does.
const KIND_LEVEL = { pattern: "easy", button: "easy", breathe: "easy", colors: "easy", controls: "easy", thermo: "easy", uart: "easy", oob: "easy",
  loop: "medium", digit: "medium", coremark: "medium", ble: "medium" };
const STATUS = { yes: "Works", partial: "On the bench" };   // what a project page records for this board; anything else is not listed

export const levelName = (l) => LEVEL_NAME[l] || "";

/** Rows for one board. recipes: the bench's recipes (all boards); projects: demos.json's `projects`; title(r): the card name. */
export function listDemos(boardId, recipes, projects, title = (r) => r.title) {
  const tier = Object.fromEntries((projects || []).map((p) => [p.slug, p.tier]));
  const guide = Object.fromEntries((projects || []).map((p) => [p.slug, p.href]));
  const rows = [];
  for (const r of recipes || []) {
    if (r.board !== boardId) continue;
    rows.push({ key: "page:" + r.id, where: "page", recipeId: r.id, title: title(r), what: r.see || r.outcome || "",
      level: tier[r.project] || KIND_LEVEL[r.check && r.check.kind] || "medium", status: "", time: "", href: guide[r.project] || "", slug: r.project || "" });
  }
  const onPage = new Set(rows.map((x) => x.slug).filter(Boolean));
  for (const p of projects || []) {
    const st = p.boards && p.boards[boardId];
    if (!STATUS[st] || onPage.has(p.slug) || !p.href) continue;
    rows.push({ key: "project:" + p.slug, where: "computer", recipeId: "", title: p.t, what: p.what || "", level: p.tier || "medium",
      status: STATUS[st], time: p.time || "", href: p.href, slug: p.slug });
  }
  return rows;
}

const norm = (s) => String(s || "").toLowerCase();

/** filters: {q, level, where}; sort: "name" | "level" | "where". Stable; ties fall back to the title. */
export function filterSort(rows, filters = {}, sort = "level") {
  const q = norm(filters.q).trim();
  const out = rows.filter((r) => (!filters.level || r.level === filters.level) && (!filters.where || r.where === filters.where)
    && (!q || norm(r.title + " " + r.what + " " + r.slug).includes(q)));
  const byName = (a, b) => a.title.localeCompare(b.title);
  const key = { name: byName, level: (a, b) => LEVELS.indexOf(a.level) - LEVELS.indexOf(b.level) || byName(a, b),
    where: (a, b) => (a.where === b.where ? 0 : a.where === "page" ? -1 : 1) || LEVELS.indexOf(a.level) - LEVELS.indexOf(b.level) || byName(a, b) }[sort] || byName;
  return out.map((r, i) => [r, i]).sort((a, b) => key(a[0], b[0]) || a[1] - b[1]).map((x) => x[0]);
}
