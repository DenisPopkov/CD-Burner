#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-build}"
CONFIG="${2:-Release}"
VERSION="${3:-0.0.0}"

cd "$ROOT"

EXE="$("$ROOT/scripts/find_cd_burner_binary.sh" "$BUILD_DIR" "$CONFIG")"
DIST="$ROOT/dist/Linux"
STAGE="$DIST/CD-Burner-$VERSION"
TARBALL="$ROOT/dist/CD-Burner-$VERSION-Linux.tar.gz"

rm -rf "$STAGE"
mkdir -p "$STAGE"

cp "$EXE" "$STAGE/CD-Burner"
chmod +x "$STAGE/CD-Burner"

rm -f "$TARBALL"
mkdir -p "$DIST"
tar -czf "$TARBALL" -C "$DIST" "CD-Burner-$VERSION"

echo ""
echo "Package ready:"
echo "  Folder: $STAGE"
echo "  Tarball: $TARBALL"
echo "Run: $STAGE/CD-Burner"
