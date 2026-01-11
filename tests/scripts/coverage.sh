#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COV_BUILD_DIR="${ROOT_DIR}/build/coverage"
cd "$ROOT_DIR"

make clean BUILD_DIR="build/coverage"
make run BUILD_DIR="build/coverage" CXXFLAGS="-std=c++17 -I../include -O0 -g --coverage" LDFLAGS="--coverage -lcrypto"

OUT_DIR="${ROOT_DIR}/coverage"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

(
  cd "$ROOT_DIR"
  gcov -b -c -o "$COV_BUILD_DIR" "$COV_BUILD_DIR"/*.gcno > /tmp/pha_cpp_gcov.txt
)

shopt -s nullglob
gcov_files=("$ROOT_DIR"/*.gcov)
if [ ${#gcov_files[@]} -gt 0 ]; then
  mv "${gcov_files[@]}" "$OUT_DIR"/
fi

python3 scripts/coverage_summary.py /tmp/pha_cpp_gcov.txt
