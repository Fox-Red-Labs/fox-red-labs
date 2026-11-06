# How to contribute: post a build, report a failure, send a change

Three routes: post a build in [Show and tell](https://github.com/Fox-Red-Labs/fox-red-labs/discussions/categories/show-and-tell), report a failure with one of the three [forms](#report-a-failure-a-build-or-a-prompt), or send a change by [pull request](#send-a-change-by-pull-request). Terms: [vocab](vocab.md). You need a GitHub account.

## Post what you built

Open [Show and tell](https://github.com/Fox-Red-Labs/fox-red-labs/discussions/categories/show-and-tell) and start a new discussion. A post has four parts:

1. **What you built or ported.** A line or two, and the project it started from.
2. **What it runs on.** The kit (its board id is the folder name), and the SDK or tool version.
3. **A photo, a clip or a log.** Something that shows it running.
4. **Anything that did not work.** Optional, and the most useful part.

The board id is the folder name, such as `mspm0g3507` or `cc2340r5`; the list is `data/board-ids.json` and `data/boards.csv`. The Show and tell page starts with three posts from the lab's own bench in the same shape. A post with all four parts can be repeated by the next person. Questions go in [Q&A](https://github.com/Fox-Red-Labs/fox-red-labs/discussions/categories/q-a).

Show and tell is a free-form post. The "Share a project" form below is the other door: it adds a row to the project's page under "Reports from others". Using both is fine.

## What you get back

- **Credit that stays.** When a post or report leads to a fix, the fix goes into the board's `FAQ.md` or `AGENTS.md` under the reporter's GitHub handle, and the handle joins that board's Built-by list, a line at the foot of its `FAQ.md` that starts with the first entry. Write "anonymous" in the post to opt out.
- **A place on the page.** Reports filed through the forms below show on the project or board page under "Reports from others", in your words, apart from the lab's measured numbers.

## Report a failure, a build or a prompt

Three forms on the repository's Issues tab take a report; each becomes a row on the page it is about, marked as yours. Blank issues are off, so the forms are the way in.

| Form | When | What it asks | What becomes of it |
|---|---|---|---|
| **Report a problem** | A step failed | The step, the exact error text pasted (not retyped), your machine and versions, what you tried | Listed on the page as reported; once it is repeated on the bench the fix goes on the page and the row says fixed |
| **Share a project** | You followed a recipe on your own board | Board and revision, project, your machine and tool versions, one proof (photo, log or measurement), what did not work | A row under "Reports from others" on that project's page |
| **A prompt that worked** | Your AI assistant did the job from words you typed | The prompt exactly as typed, the assistant and versions, what it produced and how you checked | A row in `data/community/reports.csv`, credited if you want |

Every project and board page links the three forms prefilled with that page's board and project. The rows live in `data/community/reports.csv` (`data/community/README.md` has the columns and the status words); `tools/community/issue_to_row.py` writes them from the issue, so nothing is retyped. Measurements made in this repo stay in `data/results/`; a report is never shown as one of them.

## Send a change by pull request

Fork the repository, make a branch, change the source, run `python3 tools/site_build.py` to rebuild the pages (a sparse clone needs `git sparse-checkout add tools data site` first, or clone without `--sparse`), commit with a sign-off (`git commit -s`, see [Sign-off](#sign-off)), and open a pull request. Say in the pull request which board you ran it on and which version. The sections below say what to change for each kind of work.

## Fix a page

Every site page is generated from a data file or a Markdown page; the HTML is never edited by hand. Change the source named at the bottom of the page ("Where these facts come from"), run the renderer it names in `tools/`, then `python3 tools/content_lint.py <file>` and `python3 tools/site_check.py`. The writing rules are in `docs/writing-for-ai-search.md`: lead with the answer, exact errors in backticks, versions and dates, which machine, no marketing words.

## Add a number

One row in `data/results/<board>.csv`, with its source file and how it was checked (`data/README.md`). Three runs before a median. `python3 tools/results/check.py` must print OK.

## Improve a board's AGENTS.md

Each board folder has an `AGENTS.md`, the file a reader copies into their own project so their AI assistant starts with what the runs showed. One rule: every line cites a run (a file in that folder or a row in the results store). A pull request that adds a line needs the run behind it.

## Add a board

A folder named by its id in `data/boards.csv`, with a `README.md` in the order of readme-standard.md, an `AGENTS.md` where every line cites a run, a `FAQ.md` with exact error text, and rows in `data/results/<id>.csv`. A file a second board would copy unchanged goes in `portability/` or `third-party/`. `python3 tools/readme_check.py` checks the README.

## Add a demo

Upstream code goes into `third-party/<project>/` with its `SOURCE.md`. Your own code goes into `ports/<project>/`; a port of another project keeps that project's license in its folder and says what was changed. Numbers go in as result rows in `data/results/<board>.csv`. Add an entry in `site/data/ports.json` or `site/data/projects.json`, then run `python3 tools/site_build.py` and open a pull request as described above.

## Rules this repo follows

- Public sources only. Nothing from inside any company.
- Facts about other vendors' products, or nothing. No trash talk.
- Trademarks as adjectives ("LaunchPad development kit"), never as nouns.
- No invented history, users or testimonials.
- Say "builds", "flashes", "runs" or "verified", whichever happened. Not run is not working.

## Sign-off

Every commit carries a Developer Certificate of Origin sign-off (`git commit -s`), which adds a `Signed-off-by: Your Name <email>` line. It says you wrote the change or have the right to submit it, and that you agree it is released under this repository's licenses: code under BSD-3-Clause, text, pictures and data under CC BY 4.0 (see LICENSE and NOTICE). The text of the certificate is at https://developercertificate.org/. A pull request without the line is asked for it before review.

If your employer may own what you write (most employment agreements say so for work done on company time or equipment, and some reach further), get your manager's permission before contributing and say so in the pull request in one line ("contributed with the employer's permission"). A contribution that turns out to belong to an employer that did not agree is removed. This applies to everyone, including people who work for a company whose parts appear here.

Attribution stays with you: the credit line on a page or in a report names the person who did the work, in the words they chose, and the licenses require anyone reusing the material to keep that credit.
