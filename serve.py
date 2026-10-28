#!/usr/bin/env python3
"""Braindust level editor server: serves editor.html and lets it save levels/*.json.

    python3 serve.py [port]      then open http://127.0.0.1:8000/
"""
import http.server
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
LEVELS = os.path.join(ROOT, "levels")
NAME = re.compile(r"/api/levels/([A-Za-z0-9_-]{1,64}\.json)")  # no paths, no traversal


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=ROOT, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")  # edits show up on reload
        super().end_headers()

    def reply(self, code, payload):
        body = json.dumps(payload).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/":
            self.send_response(302)
            self.send_header("Location", "/editor.html")
            self.end_headers()
            return
        if self.path == "/api/levels":
            return self.reply(200, sorted(f for f in os.listdir(LEVELS) if f.endswith(".json")))
        super().do_GET()

    def do_PUT(self):
        m = NAME.fullmatch(self.path)
        if not m:
            return self.reply(404, {"error": "expected /api/levels/<name>.json"})
        try:
            level = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
            assert isinstance(level, dict) and {"width", "height", "solids", "lurkers", "spawn", "portal", "emptyCrack"} <= level.keys()
        except (ValueError, AssertionError):
            return self.reply(400, {"error": "not a level"})
        tmp = os.path.join(LEVELS, m[1] + ".tmp")
        with open(tmp, "w") as f:
            json.dump(level, f, indent=1)
        os.replace(tmp, os.path.join(LEVELS, m[1]))  # never leave a half-written level
        self.reply(200, {"saved": m[1]})


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    os.makedirs(LEVELS, exist_ok=True)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"Braindust level editor: http://127.0.0.1:{port}/editor.html")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
