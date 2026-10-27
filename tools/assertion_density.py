# pyright: strict
# SPDX-FileCopyrightText: 2026 Gustavo Franco
# SPDX-License-Identifier: MIT

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ASSERTION = re.compile(r"\b(?:check|static_assert)\(")
DEFINITION = re.compile(r"\binline bool check\(")


@dataclass(frozen=True, slots=True)
class FileDensity:
    path: Path
    functions: int
    assertions: int


def count_functions(root: Path) -> dict[Path, int]:
    command = [
        "ctags",
        "-x",
        "--languages=C++",
        "--map-C++=+.h",
        "--kinds-C++=f",
        "--extras=-F",
        "-R",
        str(root),
    ]
    listing = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    counts: dict[Path, int] = {}
    for line in listing.splitlines():
        fields = line.split()
        if len(fields) < 4:
            continue
        path = Path(fields[3])
        counts[path] = counts.get(path, 0) + 1
    return counts


def count_assertions(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    return len(ASSERTION.findall(text)) - len(DEFINITION.findall(text))


def measure(root: Path) -> list[FileDensity]:
    functions = count_functions(root)
    headers = sorted(root.rglob("*.h"))
    return [
        FileDensity(path, functions.get(path, 0), count_assertions(path))
        for path in headers
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description="Report assertions per function.")
    parser.add_argument("--root", type=Path, default=Path("src"))
    parser.add_argument("--minimum", type=float, default=2.0)
    options = parser.parse_args()
    densities = measure(options.root)
    total_functions = sum(item.functions for item in densities)
    total_assertions = sum(item.assertions for item in densities)
    for item in densities:
        print(f"{item.assertions:4d} / {item.functions:3d}  {item.path}")
    average = total_assertions / total_functions if total_functions else 0.0
    print(
        f"average {average:.2f} assertions per function, minimum {options.minimum:.2f}"
    )
    return 0 if average >= options.minimum else 1


if __name__ == "__main__":
    sys.exit(main())
