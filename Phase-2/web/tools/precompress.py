"""Writes gzip copies of the large app files (web/static/app/*.wasm, *.js) next to them.

Run during the Render build (see render.yaml) so the small free server does not have to compress
the WebAssembly file itself. server.py uses a .gz copy only if it is newer than the original.
"""
import gzip
from pathlib import Path

app = Path(__file__).resolve().parent.parent / "static" / "app"
for path in sorted(list(app.glob("*.wasm")) + list(app.glob("*.js"))):
    data = path.read_bytes()
    packed = gzip.compress(data, 9)
    path.with_name(path.name + ".gz").write_bytes(packed)
    print(f"{path.name}: {len(data):,} -> {len(packed):,} bytes")
