#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-build}"
CONFIG="${2:-Release}"

candidates=(
  "$ROOT/$BUILD_DIR/CDBurner_artefacts/$CONFIG/CD Burner"
  "$ROOT/$BUILD_DIR/CDBurner_artefacts/CD Burner"
)

for path in "${candidates[@]}"; do
  if [[ -x "$path" ]]; then
    echo "$path"
    exit 0
  fi
done

found="$(find "$ROOT/$BUILD_DIR" -type f -name 'CD Burner' -perm -111 2>/dev/null | head -n1 || true)"
if [[ -n "$found" ]]; then
  echo "$found"
  exit 0
fi

echo "CD Burner binary not found under $BUILD_DIR ($CONFIG)" >&2
exit 1
