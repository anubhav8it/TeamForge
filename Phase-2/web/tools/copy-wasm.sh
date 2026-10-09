#!/usr/bin/env sh
# Copies the WebAssembly build of TeamForge into web/static/app (macOS/Linux).
# Usage: sh web/tools/copy-wasm.sh build-wasm
set -e
BUILD="${1:-build-wasm}"
DEST="$(dirname "$0")/../static/app"
for name in TeamForge.js TeamForge.wasm qtloader.js; do
  [ -f "$BUILD/$name" ] || { echo "Missing $BUILD/$name - did the WebAssembly build finish?"; exit 1; }
  cp "$BUILD/$name" "$DEST/"
  echo "copied $name"
done
