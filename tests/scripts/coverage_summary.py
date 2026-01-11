#!/usr/bin/env python3
import re
import sys
from pathlib import Path


def main() -> int:
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('/tmp/pha_cpp_gcov.txt')
    if not path.exists():
        print(f"Coverage summary not found: {path}")
        return 1

    data = path.read_text()
    file_re = re.compile(r"^File '([^']+)'", re.MULTILINE)
    lines_re = re.compile(r"Lines executed:([0-9.]+)% of (\d+)")

    results = []
    current_file = None
    for line in data.splitlines():
        match = file_re.match(line)
        if match:
            current_file = match.group(1)
            continue
        if current_file:
            match = lines_re.search(line)
            if match:
                pct = float(match.group(1))
                total = int(match.group(2))
                results.append((current_file, pct, total))
                current_file = None

    filtered = [(f, p, t) for (f, p, t) in results if f.startswith('../include/')]
    covered = 0.0
    lines_total = 0
    for _f, pct, total in filtered:
        lines_total += total
        covered += (pct / 100.0) * total

    if lines_total == 0:
        print('No include files matched in gcov output.')
        return 0

    agg_pct = covered / lines_total * 100.0
    print(
        f"SDK include line coverage: {agg_pct:.2f}% "
        f"({covered:.1f}/{lines_total}) across {len(filtered)} files"
    )
    print(f"Raw gcov output: {path}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
