#!/usr/bin/env python3
import argparse
import re
import sys
from pathlib import Path


TESTS_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_GCOV_TXT = Path('/tmp/pha_cpp_gcov.txt')
DEFAULT_COVERAGE_DIR = TESTS_ROOT / 'coverage'
INCLUDE_ROOT = TESTS_ROOT.parent / 'include'


def parse_summary(path: Path):
    if not path.exists():
        print(f"Coverage summary not found: {path}")
        return []

    text = path.read_text()
    file_re = re.compile(r"^File '([^']+)'", re.MULTILINE)
    lines_re = re.compile(r"Lines executed:([0-9.]+)% of (\d+)")

    results = []
    current_file = None
    for line in text.splitlines():
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

    return results


def normalize_path(path_str: str) -> Path:
    p = Path(path_str)
    if p.is_absolute():
        return p
    return (TESTS_ROOT / p).resolve()


def is_sdk_include(path_str: str) -> bool:
    abs_path = normalize_path(path_str)
    return INCLUDE_ROOT in abs_path.parents


def summarize_coverage(summary, top_n: int):
    include_items = []
    total_lines = 0
    covered_lines = 0.0

    for path_str, pct, total in summary:
        if not is_sdk_include(path_str):
            continue
        abs_path = normalize_path(path_str)
        rel_path = abs_path.relative_to(INCLUDE_ROOT)
        include_items.append((rel_path.as_posix(), pct, total))
        total_lines += total
        covered_lines += (pct / 100.0) * total

    if total_lines == 0:
        print('No SDK include files found in coverage summary.')
        return

    overall = covered_lines / total_lines * 100.0
    print(f"SDK include line coverage: {overall:.2f}% ({covered_lines:.1f}/{total_lines})")
    print(f"Files considered: {len(include_items)}")
    print('Lowest coverage files:')

    for rel_path, pct, total in sorted(include_items, key=lambda x: x[1])[:top_n]:
        print(f"  {pct:6.2f}% ({total:5d} lines)  {rel_path}")


def list_uncovered_lines(gcov_dir: Path, file_filter: str, max_lines: int):
    if not gcov_dir.exists():
        print(f"Coverage directory not found: {gcov_dir}")
        return 1

    matches = [p for p in gcov_dir.glob('*.gcov') if file_filter.lower() in p.name.lower()]
    if not matches:
        print(f"No .gcov files matching '{file_filter}' in {gcov_dir}")
        return 1

    if len(matches) > 1:
        print('Multiple matches found. Be more specific:')
        for match in matches:
            print(f"  {match.name}")
        return 1

    gcov_path = matches[0]
    uncovered = []
    pattern = re.compile(r"^\s*#####:\s*(\d+):(.*)$")

    for line in gcov_path.read_text().splitlines():
        match = pattern.match(line)
        if match:
            line_no = int(match.group(1))
            code = match.group(2).strip()
            uncovered.append((line_no, code))

    if not uncovered:
        print(f"No uncovered lines found in {gcov_path.name}")
        return 0

    print(f"Uncovered lines in {gcov_path.name} (showing up to {max_lines}):")
    for line_no, code in uncovered[:max_lines]:
        print(f"  {line_no:6d}: {code}")

    if len(uncovered) > max_lines:
        print(f"  ... {len(uncovered) - max_lines} more")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description='Analyze gcov summary for SDK coverage.')
    parser.add_argument('--summary', default=str(DEFAULT_GCOV_TXT), help='Path to gcov summary output')
    parser.add_argument('--top', type=int, default=15, help='How many lowest-coverage files to list')
    parser.add_argument('--file', dest='file_filter', help='Show uncovered lines for a specific .gcov file')
    parser.add_argument('--max-lines', type=int, default=30, help='Max uncovered lines to print')
    args = parser.parse_args()

    summary = parse_summary(Path(args.summary))
    if summary:
        summarize_coverage(summary, args.top)
        print(f"Raw gcov output: {args.summary}")

    if args.file_filter:
        return list_uncovered_lines(DEFAULT_COVERAGE_DIR, args.file_filter, args.max_lines)

    print("Tip: use --file <name> to list uncovered lines from tests/coverage")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
