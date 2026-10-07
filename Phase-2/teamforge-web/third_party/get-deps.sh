#!/bin/sh
# Downloads the two libraries TeamForge needs into this folder.
# Usage: sh third_party/get-deps.sh   (needs curl and unzip)
set -e
cd "$(dirname "$0")"
HTTPLIB="https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.15.3/httplib.h"
SQLITE="https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip"

echo "Downloading cpp-httplib..."
curl -fsSL "$HTTPLIB" -o httplib.h
echo "Downloading SQLite..."
curl -fsSL "$SQLITE" -o sqlite.zip
rm -rf sqlite-tmp && unzip -q sqlite.zip -d sqlite-tmp
cp sqlite-tmp/*/sqlite3.c sqlite-tmp/*/sqlite3.h .
rm -rf sqlite.zip sqlite-tmp
echo "Done: httplib.h, sqlite3.c, sqlite3.h"
