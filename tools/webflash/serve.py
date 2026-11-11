#!/usr/bin/env python3
"""Serve a pulled checkout on loopback, where Chrome/Edge permit USB access."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlencode

ROOT = Path(__file__).resolve().parents[2]


class BenchHandler(SimpleHTTPRequestHandler):
    # Windows registry MIME mappings can otherwise serve modules as text/plain.
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map,
                      '.js': 'text/javascript', '.mjs': 'text/javascript',
                      '.json': 'application/json', '.out': 'application/octet-stream'}

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--board', default='')
    args = parser.parse_args()
    handler = partial(BenchHandler, directory=str(ROOT))
    with ThreadingHTTPServer(('127.0.0.1', args.port), handler) as server:
        suffix = '?' + urlencode({'board': args.board}) if args.board else ''
        print(f'Open http://localhost:{server.server_port}/site/flash/bench/{suffix} in Chrome or Edge.', flush=True)
        print('The board must be plugged into this computer. Ctrl+C stops the server.', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
