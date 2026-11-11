"""Inventory-wide bench entry points. USB chip recognition remains in boards.json."""
import csv
from pathlib import Path

# Inventory IDs whose source folder has a different name.
FOLDERS = {'cc3200': 'cc3200-launchxl', 'mspm0g3507': 'mspm0g3507',
           'cc2340r5': 'cc2340r5', 'f2800137': 'c2000-launchpad',
           'f28p65x': 'c2000-launchpad'}

# A few site board pages use the exact chip or kit revision rather than the inventory family ID.
SITE_PAGES = {'cc35x1': 'board-cc3551e.html'}


def catalog(root, boards, recipes):
    known = {b['id']: b for b in boards}
    rows = list(csv.DictReader((root / 'data/boards.csv').open()))
    entries = []
    for row in rows:
        bid = row['board']
        b = known.get(bid)
        demos = [r for r in recipes if r['board'] == bid]
        can_write = bool(b and (b.get('programmable') or b.get('saci')) and demos)
        folder = FOLDERS.get(bid, bid)
        readme = f'{folder}/README.md'
        if not (root / readme).is_file():
            readme = 'docs/inventory.md'
        page = SITE_PAGES.get(bid, f'board-{bid}.html')
        entries.append({'id': bid, 'kit': row['name'], 'recognized': b is not None,
                        'browserFlash': can_write, 'demos': len(demos), 'readme': readme,
                        'page': page if (root / 'site' / page).is_file() else None,
                        'status': ('Browser demos available' if can_write else
                                   'Recognized; browser flashing unavailable' if b else
                                   'Use the board instructions; no browser connection yet'),
                        'note': b.get('note', '') if b else 'This page does not identify or flash this board over USB. Use its existing board notes and instructions.'})
    # New browser adapters must remain reachable even before an inventory row is added.
    inventory = {e['id'] for e in entries}
    for b in boards:
        if b['id'] not in inventory:
            raise ValueError(f"Browser board {b['id']} needs a data/boards.csv inventory row")
    return entries
