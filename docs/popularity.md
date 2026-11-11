# Popularity on the builds page

Hearts, a view count and a Digital Lab flash count per build, and a sort control on `projects.html` that appears once there is something to sort by. The way Arduino's project hub and Hackster do it, with no accounts.

## What is counted

| Counter | Meaning | Who sends it |
|---|---|---|
| `views` | A build page was loaded and shown | `site/popularity.js` on `project-*.html`, once per load |
| `flashes` | The Digital Lab wrote that build's program to a board and the check passed | `site/flash/bench/bench.js`, keyed by the recipe's `project` in `recipes.json` |
| `likes` | A heart on a card or a build page | `site/popularity.js`, on a tap |

Not counted: prefetched or prerendered pages, background tabs until they are shown, and any browser driven by automation (`navigator.webdriver`), so our own checks add nothing. A recipe with no build slug is skipped.

## What is stored

Only counts, per-day counts and hashed like keys, all in one Cloudflare KV namespace.

- `c:{views|flashes|likes}:{slug}`: the running total (the number sits in the key's metadata).
- `d:{YYYY-MM-DD}:{slug}`: views and flashes for one day, kept 8 days. `GET /counts?window=7` adds the last 7 days, which is the Trending sort (views plus 3 times flashes).
- `l:{hash}`: one like. The hash is HMAC-SHA-256, keyed by the `SALT` secret, of the caller's address and the slug; it holds nothing readable, expires after a year, and lets a second tap from the same address take the like back. No address, cookie or browser id is stored.
- Rate limit: at most 20 writes per address per minute, enforced by Cloudflare's Rate Limiting binding (`LIMITER`, keyed by an HMAC of the address, nothing stored by us). Without the binding the worker falls back to an approximate KV counter (`r:{hash}:{minute}`), a soft cap only.

The browser keeps the slugs you hearted in `localStorage` (`frl-liked`) so the heart stays filled. Nothing else is written there.

## What the page shows

Nothing, until the worker answers. Hearts and the sort control are in the HTML with the `hidden` attribute and `popularity.js` reveals them after `GET /counts` succeeds. The sort control stays hidden while every count is zero, so the first day shows no numbers; there are none seeded. The count beside a heart is hidden while it is zero. If the worker is down the page looks as it did before; the browser may still log one failed network request, which a script cannot suppress.

Sorts: Default (the page's own order), Most viewed, Most flashed, Most liked, Trending (7 days). They reorder cards inside each subject shelf.

Counts come from a 60-second edge cache, so another person's like can take up to a minute to show. Your own tap updates the page at once from the worker's reply. KV has no atomic add, so two writes in the same instant can lose one count; at this traffic that does not matter.

## The worker

`tools/popularity/worker/` (no dependencies). Endpoints:

- `GET /counts[?window=7]`
- `POST /hit/views/{slug}` and `POST /hit/flashes/{slug}`
- `POST /like/{slug}[?set=1|0]` (a toggle; `set` makes it idempotent, which the page uses so a cleared `localStorage` cannot flip a like by accident)

Writes need an `Origin` of `https://foxredlabs.dev` or `https://preview.foxredlabs.dev`, plus any in the `EXTRA_ORIGINS` secret (the Pages test domain). Slugs must match `^[a-z0-9-]{1,64}$` and be in `worker/slugs.js`, which `tools/popularity/gen_slugs.py` writes from `site/data/projects.json` (`--check` fails when stale; redeploy the worker after a new build is added).

Run it locally and point the page at it:

    cd tools/popularity/worker
    npx wrangler dev --local --port 8788 --var EXTRA_ORIGINS:http://127.0.0.1:8827
    # in the browser console before the page loads: window.FRL_POP_API = 'http://localhost:8788'; window.FRL_POP_COUNT_ROBOTS = true

Deploy (needs a Cloudflare login with Workers and KV rights on the zone):

    cd tools/popularity/worker
    npx wrangler kv namespace create COUNTS     # once; put its id in wrangler.toml
    npx wrangler secret put SALT                # once; 32 or more random characters. Without it every write gets 503
    npx wrangler secret put EXTRA_ORIGINS       # optional
    npx wrangler deploy                         # also binds counts.foxredlabs.dev and the rate limiter
    python3 tools/popularity/deploy_check.py    # fails when SALT is missing

Tests: `node --test tools/popularity/worker/worker.test.mjs` (the worker, with a fake KV) and `tools/tests/test_popularity.py` (the rendered hearts and sort control, the slug list).

The address the page calls is one constant at the top of `site/popularity.js`.

## In the copies

Popularity is not analytics: it is an opt-in tap plus a view count with no identifier, so the open copy keeps `popularity.js` (`tools/publish/open_apex.py` drops only `frl-ev.js` and `functions/`).
