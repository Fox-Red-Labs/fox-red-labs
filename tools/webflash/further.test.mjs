// Each bench demo's "Go further" leads to that demo, not another one. No browser, no board.
//   node --test tools/webflash/further.test.mjs
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const recipes = JSON.parse(readFileSync(join(ROOT, "site/flash/bench/recipes.json"), "utf8")).recipes;
const routes = JSON.parse(readFileSync(join(ROOT, "data/routes.json"), "utf8")).boards;

test("every demo has a go-further block naming its own source", () => {
  for (const r of recipes) {
    assert.ok(r.further, `${r.board}/${r.id}`);
    assert.ok(r.further.command, `${r.board}/${r.id}: no command`);
    assert.ok(r.further.source.length, `${r.board}/${r.id}: no source`);
    for (const f of r.further.source) {
      const path = f.split(" ")[0];
      if (path.includes("/")) assert.ok(existsSync(join(ROOT, path)), `${r.board}/${r.id}: ${path} does not exist`);
    }
  }
});

test("a guided command only where ./start.sh has a route for that recipe on that board", () => {
  for (const r of recipes.filter((x) => x.further.route === "guided")) {
    assert.ok(routes[r.board] && routes[r.board].recipes[r.recipe], `${r.board}/${r.recipe} is marked guided with no route`);
  }
});

test("no demo but the blink sends people to the blink", () => {
  for (const r of recipes.filter((x) => x.recipe !== "blink-pattern")) {
    assert.doesNotMatch(r.further.command, /start\.sh \S+ blink\b/, `${r.board}/${r.id} hands over to blink`);
    assert.doesNotMatch(r.terminal, /start\.sh \S+ blink\b/, `${r.board}/${r.id} terminal hands over to blink`);
  }
});

test("the build command named for an image is the one its manifest records", () => {
  for (const r of recipes.filter((x) => x.further.route === "build" && x.manifest)) {
    const m = JSON.parse(readFileSync(join(ROOT, "site/flash/bench", r.manifest), "utf8"));
    assert.ok(m.source.rebuild.startsWith(r.further.command), `${r.board}/${r.id}`);
  }
});

test("the GPL-3.0 digit reader says so, on the page and in the NOTICE shipped with its image", () => {
  for (const r of recipes.filter((x) => x.check.kind === "digit")) {
    assert.match(r.further.license, /GPL-3\.0/);
    const notice = readFileSync(join(ROOT, "site/flash/bench", r.further.notice), "utf8");
    assert.match(notice, /BitNetMCU/, `${r.board}: NOTICE.txt does not name BitNetMCU`);
    assert.match(notice, /GNU GENERAL PUBLIC LICENSE/, `${r.board}: NOTICE.txt lacks the GPL text`);
  }
});
