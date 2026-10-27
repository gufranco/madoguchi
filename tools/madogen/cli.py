# pyright: strict
# SPDX-FileCopyrightText: 2026 Gustavo Franco
# SPDX-License-Identifier: MIT

import argparse
import base64
import getpass
import hashlib
import json
import os
import sys
from pathlib import Path
from typing import cast

from .assets import AssetError, collect_assets
from .emit import render_api, same_output, snake
from .provision import (
    DEFAULT_ITERATIONS,
    SecretsError,
    is_placeholder,
    make_secrets,
    placeholder,
    render_secrets,
)
from .spec import Api, Json, SpecError, parse_api

SPEC_FILE = "openapi.json"
SITE_FOLDER = "www"
OUTPUT_FILE = "generated_api.h"
SECRETS_FILE = "arduino_secrets.h"


def load_api(folder: Path) -> tuple[Json, Api]:
    raw = json.loads((folder / SPEC_FILE).read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        raise SpecError("", "the document must be an object")
    document = cast(Json, raw)
    return document, parse_api(document)


def render_sketch(folder: Path) -> str:
    document, api = load_api(folder)
    served = json.dumps(document, separators=(",", ":"), ensure_ascii=False).encode(
        "utf-8"
    )
    namespace = snake(folder.resolve().name)
    assets = collect_assets(folder / SITE_FOLDER, served, api.buffers.document_max_age)
    return render_api(api, assets, namespace)


def generate(folders: list[Path]) -> int:
    for folder in folders:
        (folder / OUTPUT_FILE).write_text(render_sketch(folder), encoding="utf-8")
        secured = any(operation.secured for operation in load_api(folder)[1].operations)
        secrets_path = folder / SECRETS_FILE
        if secured and not secrets_path.exists():
            secrets_path.write_text(render_secrets(placeholder()), encoding="utf-8")
        print(f"wrote {folder / OUTPUT_FILE}")
    return 0


def check_folder(folder: Path) -> list[str]:
    output = folder / OUTPUT_FILE
    if not output.exists():
        return [f"{output} is missing; run: python3 -m tools.madogen generate {folder}"]
    problems: list[str] = []
    if not same_output(render_sketch(folder), output.read_text(encoding="utf-8")):
        problems.append(
            f"{output} is stale; run: python3 -m tools.madogen generate {folder}"
        )
    secrets_path = folder / SECRETS_FILE
    if secrets_path.exists() and not is_placeholder(
        secrets_path.read_text(encoding="utf-8")
    ):
        problems.append(
            f"{secrets_path} holds a real key; restore the all-zero placeholder"
        )
    return problems


def check(folders: list[Path]) -> int:
    problems = [problem for folder in folders for problem in check_folder(folder)]
    for problem in problems:
        print(problem)
    return 1 if problems else 0


def read_password(from_stdin: bool) -> str:
    if from_stdin:
        return sys.stdin.readline().rstrip("\n")
    return getpass.getpass("Password: ")


def provision(folder: Path, user: str, iterations: int, from_stdin: bool) -> int:
    secrets = make_secrets(user, read_password(from_stdin), iterations, os.urandom)
    (folder / SECRETS_FILE).write_text(render_secrets(secrets), encoding="utf-8")
    print(f"wrote {folder / SECRETS_FILE}; never commit it")
    return 0


def sri(file: Path) -> int:
    digest = base64.b64encode(hashlib.sha384(file.read_bytes()).digest()).decode(
        "ascii"
    )
    print(f"sha384-{digest}")
    return 0


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(
        prog="madogen", description="Build-time code for Madoguchi sketches."
    )
    commands = root.add_subparsers(dest="command", required=True)
    for name in ("generate", "check"):
        command = commands.add_parser(name)
        command.add_argument("sketches", nargs="+", type=Path)
    secrets = commands.add_parser("secrets")
    secrets.add_argument("sketch", type=Path)
    secrets.add_argument("--user", required=True)
    secrets.add_argument("--iterations", type=int, default=DEFAULT_ITERATIONS)
    secrets.add_argument("--password-stdin", action="store_true")
    integrity = commands.add_parser("sri")
    integrity.add_argument("file", type=Path)
    return root


def run(options: argparse.Namespace) -> int:
    command = str(options.command)
    if command == "generate":
        return generate(cast(list[Path], options.sketches))
    if command == "check":
        return check(cast(list[Path], options.sketches))
    if command == "secrets":
        return provision(
            cast(Path, options.sketch),
            str(options.user),
            int(options.iterations),
            bool(options.password_stdin),
        )
    return sri(cast(Path, options.file))


def main(argv: list[str]) -> int:
    options = parser().parse_args(argv)
    try:
        return run(options)
    except (
        SpecError,
        AssetError,
        SecretsError,
        OSError,
        json.JSONDecodeError,
    ) as error:
        print(f"madogen: {error}", file=sys.stderr)
        return 2
