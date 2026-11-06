# Use Fox Red Labs notes and measurements in your own project and assistant

Terms: [vocab](vocab.md). Take the example, measurement or explanation you need. Read it yourself, give the relevant files to your assistant, or connect that assistant to the local catalogue. A complete build is optional.

## Find an answer or a working piece

The [index](../llms.txt) points to build notes, measurements, board context and fixes. A build's Markdown lives in `docs/builds/`; its page links the implementation. Each board folder has its own notes and FAQ. Measurements live in `data/results/`, with the method and a source field.

Keep the tested board, software versions, dependencies, pin assumptions and limits with code you reuse. Check the relevant source license before copying. A run on the lab's setup is evidence about that setup; an adaptation still needs checking.

## Bring context into your project

Save the relevant build notes and board context alongside your project, for example under `references/fox-red-labs/`. Keep your existing project instructions. Ask your assistant explicitly to read the reference files; you do not need to replace its `AGENTS.md` or `CLAUDE.md`.

Example files for adapting an LED pattern:

- [Build notes](builds/blink.md).
- [L1306 board context](../mspm0l1306/AGENTS.md) and [known errors](../mspm0l1306/FAQ.md).

Replace the task below and name the files you saved:

```text
Read the Fox Red Labs references I attached. My task is: <describe your task>. Identify the smallest useful piece, its dependencies and tested conditions. Explain what I need to change for my setup and how to check it. Cite the source and state what is untested. Do not install tools or write to a board.
```

Board notes contain relative links to the repository. Keep the source link and revision with your copy so you can follow those references. The current code and recorded result are the reference for what worked.

## Connect the local catalogue

Requirements: Python 3.9 or later, a local repository copy with its catalogue data, and an MCP-capable assistant that can start a local process. No board, compiler or SDK is needed for catalogue lookup.

From the root of an existing clone, a starter sparse checkout can add the catalogue files:

```sh
git sparse-checkout add tools/frl data docs/builds site/data
```

Use that command only if this is a sparse checkout. A full clone already has those directories. The catalogue needs `data/boards.csv`, `data/results/`, `data/parts.json` and `site/data/projects.json`; the docs provide the explanations linked by tool results. Code and raw evidence referenced by a result may live elsewhere in the repository.

Replace `/full/path/to/fox-red-labs` below with your repository's absolute path. Keep the quotes if it contains spaces. Run the command for the assistant you use, then open a fresh session in that project.

Codex:

```sh
codex mcp add frl-library -- python3 "/full/path/to/fox-red-labs/tools/frl/mcp_server.py" --catalog-only
```

Claude Code: first open a terminal in **your own project**, where you want the connector available. Local scope applies only to the project where you add it. Keep the server path pointing to the Fox Red Labs clone.

```sh
cd "/full/path/to/your-project"
```

Then add the project-local connection:

```sh
claude mcp add --scope local frl-library -- python3 "/full/path/to/fox-red-labs/tools/frl/mcp_server.py" --catalog-only
```

Other clients: configure a local stdio server with command `python3` and arguments `[/full/path/to/fox-red-labs/tools/frl/mcp_server.py, --catalog-only]`. In a JSON configuration, make each argument a quoted string. A remote/cloud assistant cannot assume access to files on your computer.

The `--catalog-only` option offers exactly five tools and rejects board-operation calls, including when `FRL_MCP_TOOLS` names board tools. The process reads the local files and does not update them from the network. Your assistant processes returned content under its own data settings.

| Tool | Use it to |
|---|---|
| `site_boards` | List board properties and available result files |
| `site_results` | Read one board's benchmark rows, optionally filtered by test |
| `site_builds` | Find builds and their recorded board status |
| `site_build` | Retrieve one build's parts, pins, steps and limits |
| `site_parts` | Look up parts and the builds that use them |

Start with a small check:

```text
Call site_builds through frl-library. Pick the blink build and call site_build for it. Give me its source path and its stated board status and limits. Do not install anything or write to hardware.
```

Then ask about your own task. Check sources and recorded limits before using an answer. A build listing is not proof that the full route is supported in your environment.

## Freshness, incomplete results and setup errors

The catalogue answers from this local copy. Record the revision with `git rev-parse HEAD`; a modified working tree can differ from that revision. Update using your normal Git workflow. Do not discard local changes to refresh the catalogue.

`site_results` returns at most 200 rows and sets `truncated: true` when more exist. Filter by a test ID or read the named CSV for the full set. Do not calculate a whole-board comparison from an incomplete response.

If a call says `this copy has no ...`, add the missing catalogue data to the sparse checkout. If the assistant lists no tools, check its MCP connection status and confirm the configured Python command and absolute path exist. Running the server alone waits for protocol messages; silence is not a completed lookup.

## What is verified

The catalogue-only process and its permission boundary are covered by automated protocol tests, including rejection of board commands and result-file paths outside the catalogue. The prepared release export was also queried for boards, results, builds and parts.

On October 16, 2026, Codex CLI 0.153.4 on an Intel Mac called `site_builds` and `site_build` through the catalogue-only connection and returned the blink source, board status and a stated limit. This used temporary process configuration. The permanent `mcp add` installation and desktop chat interfaces have not been exercised by this check. Claude Code setup remains unverified. No customer task completion or hardware operation is implied.

For building, flashing and recovery, use the separate [local board-tool guide](frl.md) and recorded assistant routes. Those operations require their own prerequisites and permission.

Client setup references: [Claude Code MCP documentation](https://code.claude.com/docs/en/mcp). The Codex command form should match the installed `codex mcp add --help`; client settings and approvals can vary by version.
