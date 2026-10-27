# pyright: strict
# SPDX-FileCopyrightText: 2026 Gustavo Franco
# SPDX-License-Identifier: MIT

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any, cast


def section_problems(sections: list[dict[str, Any]], min_free_ram: int) -> list[str]:
    problems: list[str] = []
    for section in sections:
        name = str(section["name"])
        size = int(section["size"])
        limit = int(section["max_size"])
        if size > limit:
            problems.append(f"{name} uses {size} of {limit} bytes")
        if name == "data" and (limit - size) < min_free_ram:
            problems.append(
                f"data leaves {limit - size} bytes free, needs {min_free_ram}"
            )
    return problems


def warning_problems(errors: str, own_root: str) -> list[str]:
    lines = [line.strip() for line in errors.splitlines()]
    return [line for line in lines if "warning:" in line and line.startswith(own_root)]


def evaluate(report: dict[str, Any], min_free_ram: int, own_root: str) -> list[str]:
    if not report.get("success", False):
        return ["build failed"]
    builder = cast(dict[str, Any], report.get("builder_result", {}))
    sections = cast(list[dict[str, Any]], builder.get("executable_sections_size", []))
    if not sections:
        return ["no section sizes in the build report"]
    problems = section_problems(sections, min_free_ram)
    problems.extend(warning_problems(str(report.get("compiler_err", "")), own_root))
    return problems


def compile_sketch(fqbn: str, sketch: Path, library: Path) -> dict[str, Any]:
    command = [
        "arduino-cli",
        "compile",
        "--fqbn",
        fqbn,
        "--warnings",
        "all",
        "--clean",
        "--library",
        str(library),
        "--format",
        "json",
        str(sketch),
    ]
    completed = subprocess.run(command, check=False, capture_output=True, text=True)
    return cast(dict[str, Any], json.loads(completed.stdout or "{}"))


def main() -> int:
    parser = argparse.ArgumentParser(description="Fail a build over its size budget.")
    parser.add_argument("--fqbn", required=True)
    parser.add_argument("--min-free-ram", type=int, default=0)
    parser.add_argument("sketches", nargs="+", type=Path)
    options = parser.parse_args()
    library = Path.cwd()
    failed = False
    for sketch in options.sketches:
        result = compile_sketch(options.fqbn, sketch, library)
        problems = evaluate(result, options.min_free_ram, str(library))
        status = "ok" if not problems else "FAIL"
        print(f"{status} {options.fqbn} {sketch}")
        for problem in problems:
            print(f"  {problem}")
        failed = failed or bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
