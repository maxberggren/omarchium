#!/usr/bin/env python3
"""Look-and-feel preview server for Omarchium.

Serves preview/ plus /api/data: every installed Omarchy theme's colors.toml,
the active theme, and the color map the browser would use. The page polls it,
so editing the map or switching Omarchy themes updates the preview live.

  scripts/preview [--port 8787]
"""

import argparse
import hashlib
import http.server
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HOME = Path.home()
STATE = Path(os.environ.get("XDG_STATE_HOME", HOME / ".local/state")) / "omarchy/current"
CONFIG = Path(os.environ.get("XDG_CONFIG_HOME", HOME / ".config"))
THEME_DIRS = [Path(os.environ.get("OMARCHY_PATH", "/usr/share/omarchy")) / "themes",
              CONFIG / "omarchy/themes"]
DEFAULT_MAP = ROOT / "core/resources/omarchy_color_map.json"
USER_MAP = CONFIG / "omarchium/color-map.json"


def read(path):
    try:
        return path.read_text()
    except OSError:
        return None


def hyprland_rounding():
    try:
        out = subprocess.run(["hyprctl", "getoption", "decoration:rounding", "-j"],
                             capture_output=True, text=True, timeout=2).stdout
        return json.loads(out).get("int")
    except (OSError, ValueError, subprocess.SubprocessError):
        return None


def omarchy_font():
    try:
        out = subprocess.run(["fc-match", "monospace", "-f", "%{family}"],
                             capture_output=True, text=True, timeout=2).stdout
        return out.split(",")[0].strip() or None
    except (OSError, subprocess.SubprocessError):
        return None


def collect():
    themes = {}
    for d in THEME_DIRS:
        if d.is_dir():
            for t in sorted(d.iterdir()):
                toml = read(t / "colors.toml")
                if toml:
                    themes[t.name] = toml  # user themes override built-in ones
    current = (read(STATE.parent / "current/theme.name") or "").strip()
    current_toml = read(STATE / "theme/colors.toml")
    if current_toml:
        themes[current or "current"] = current_toml
    map_path = USER_MAP if USER_MAP.is_file() else DEFAULT_MAP
    data = {
        "themes": themes,
        "current": current,
        "map": read(map_path),
        "mapPath": str(map_path).replace(str(HOME), "~"),
        "hyprlandRounding": hyprland_rounding(),
        "omarchyFont": omarchy_font(),
    }
    blob = json.dumps(data, sort_keys=True)
    data["version"] = hashlib.sha1(blob.encode()).hexdigest()[:12]
    return data


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=str(ROOT / "preview"), **kw)

    def do_GET(self):
        if self.path.startswith("/api/data"):
            body = json.dumps(collect()).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
            return
        if self.path.startswith("/core/"):
            self.directory = str(ROOT)
        super().do_GET()

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, *a):
        pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8787)
    args = ap.parse_args()
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"Omarchium preview: http://localhost:{args.port}", flush=True)
    srv.serve_forever()


if __name__ == "__main__":
    main()
