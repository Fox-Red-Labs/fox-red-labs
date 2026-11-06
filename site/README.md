# The site folder: generated pages, do not edit by hand

Terms: [vocab](../docs/vocab.md).

Static HTML for the Fox Red Labs pages (projects, boards, benchmarks, FAQs), the data they are built from (`data/*.json`), fonts, pictures and the vendored scripts the pages run (`vendor/`, each with its license). Open `index.html` through a local server (`python3 -m http.server --bind 127.0.0.1` from the top of the repository), not as a `file:` URL, because the tools load modules.

Change the data or the renderer, not the HTML: `python3 tools/site_build.py build`, then `python3 tools/site_check.py` must print OK. Not here: the private notes the pages were written from. Each project page also exists as plain Markdown in `docs/builds/`.
