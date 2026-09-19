"""Serve the local WebAssembly build; no Internet-facing listener."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import webbrowser


class WebBuildHandler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm"}

    def end_headers(self) -> None:
        # Rebuilds must not mix a cached JS loader with a new WASM binary.
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, default=Path(__file__).resolve().parents[1] / "build_web")
    parser.add_argument("--port", type=int, default=8123)
    parser.add_argument("--open-browser", action="store_true")
    args = parser.parse_args()
    directory = args.directory.resolve()
    if not (directory / "game.html").is_file():
        parser.error(f"No game.html found in {directory}; run build_web.ps1 first.")
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    handler = partial(WebBuildHandler, directory=str(directory))
    try:
        server = ThreadingHTTPServer(("127.0.0.1", args.port), handler)
    except OSError as error:
        parser.exit(1, f"Unable to start local server: {error}\n")
    url = f"http://127.0.0.1:{args.port}/game.html"
    print(f"Game: {url}\nServing: {directory}\nPress Ctrl+C to stop.", flush=True)
    if args.open_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
