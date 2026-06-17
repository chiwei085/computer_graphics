#!/usr/bin/env python3
"""Amalgamate lab12 into a single self-contained shadow.cpp.

Usage:
    python3 gen.py          # writes shadow.cpp next to this script
"""

from pathlib import Path
import re

ROOT = Path(__file__).parent.parent   # src/lab12
INC  = ROOT / "include"
SRC  = ROOT / "src"
OUT  = Path(__file__).parent / "shadow.cpp"

# ── inclusion order matters: dependencies before dependents ──────────────────
HEADERS = [
    INC / "constants.hpp",    # lab12::constants — must precede all users
    INC / "math3d.hpp",
    INC / "gl_resources.hpp",
    INC / "vertex.hpp",
    INC / "scene_renderer.hpp",
    INC / "overlay_renderer.hpp",
    INC / "app.hpp",
]

SOURCES = [
    SRC / "app.cpp",
    SRC / "main.cpp",
]

# ── regex patterns ────────────────────────────────────────────────────────────
_LOCAL  = re.compile(r'^\s*#\s*include\s+"[^"]+"\s*$', re.M)
_PRAGMA = re.compile(r'^\s*#\s*pragma\s+once\s*\n', re.M)
_SYS    = re.compile(r'^\s*(#\s*include\s+<[^>]+>)\s*$', re.M)
_BARE   = re.compile(r'^\s*(#\s*include\s+[A-Z_][A-Z0-9_]*)\s*$', re.M)  # FT_FREETYPE_H

def collect(text: str) -> tuple[list[str], str]:
    """Return (system include lines, stripped body)."""
    includes = [m.group(1) for m in _SYS.finditer(text)]
    includes += [m.group(1) for m in _BARE.finditer(text)]

    body = _PRAGMA.sub('', text)
    body = _LOCAL.sub('', body)
    body = _SYS.sub('', body)
    body = _BARE.sub('', body)
    body = re.sub(r'\n{3,}', '\n\n', body).strip()
    return includes, body


seen_set:  set[str]  = set()
sys_incs:  list[str] = []
sections:  list[str] = []

for path in HEADERS + SOURCES:
    incs, body = collect(path.read_text())
    for inc in incs:
        if inc not in seen_set:
            sys_incs.append(inc)
            seen_set.add(inc)
    if body:
        bar = '─' * 62
        sections.append(f"// {bar}\n// {path.name}\n// {bar}\n{body}")

compile_hint = """\
// Compile (Linux / WSL):
//   g++ -std=c++17 shadow.cpp -o shadow \\
//       -lGL -lGLU -lglut $(pkg-config --libs --cflags freetype2)
"""

parts: list[str] = [
    "// AUTO-GENERATED — do not edit directly; run gen.py to regenerate\n",
    compile_hint,
    *sys_incs,
    "",
    *[s + "\n" for s in sections],
]

output = "\n".join(parts)
OUT.write_text(output)
lines = output.count('\n')
print(f"[gen] {OUT.relative_to(ROOT.parent.parent)}  "
      f"({OUT.stat().st_size:,} bytes, {lines} lines)")
