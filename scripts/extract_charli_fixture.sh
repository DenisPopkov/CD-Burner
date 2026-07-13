#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${1:-$ROOT/Tests/Fixtures/sources/Charli-XCX-SS26-Rock-Music.flac}"
OUT="$ROOT/Tests/Fixtures/Charli-XCX-SS26-Rock-Music.fixture.flac"

if [[ ! -f "$SRC" ]]; then
  echo "Source not found: $SRC" >&2
  echo "Place the full track at Tests/Fixtures/sources/Charli-XCX-SS26-Rock-Music.flac" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUT")"
ffmpeg -y -hide_banner -loglevel error -ss 30 -t 90 -i "$SRC" -c:a flac "$OUT"
ls -lh "$OUT"
