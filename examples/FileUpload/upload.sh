#!/bin/sh
set -eu

usage="usage: sh upload.sh BOARD [FILE ...]"
[ "$#" -ge 1 ] || { echo "$usage" >&2; exit 64; }
target="http://$1/upload"
shift
[ "$#" -gt 0 ] || set -- html/index.htm html/sample.svg

for path in "$@"; do
  printf 'sending %s\n' "$path"
  curl -fsS -T "$path" "$target/$(basename "$path")"
done
