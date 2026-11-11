import test from "node:test";
import assert from "node:assert/strict";
import { openTakeHome } from "./dlab-scenarios.mjs";

// "Take it home" starts folded on the page, which hides its Copy buttons; the scenarios must open it before clicking them.
test("openTakeHome unfolds the block, then scrolls the terminal Copy button into view", async () => {
  const calls = [];
  const el = { classList: { remove: (c) => calls.push("remove:" + c) } };
  globalThis.document = { getElementById: (id) => (id === "takehome" ? el : null) };
  const page = {
    evaluate: async (fn) => { fn(); calls.push("evaluate"); },
    locator: (sel) => ({ scrollIntoViewIfNeeded: async () => calls.push("scroll:" + sel) }),
  };
  await openTakeHome(page);
  delete globalThis.document;
  assert.deepEqual(calls, ["remove:folded", "evaluate", "scroll:#th-term-copy"]);
});
