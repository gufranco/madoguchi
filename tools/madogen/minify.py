# pyright: strict
# SPDX-FileCopyrightText: 2026 Gustavo Franco
# SPDX-License-Identifier: MIT

import json
import re
import shutil
import subprocess
import tempfile
from collections.abc import Callable
from pathlib import Path

HTML_COMMENT = re.compile(r"<!--(?!\[).*?-->", re.DOTALL)
HTML_KEEP = re.compile(r"<(pre|textarea|script)\b.*?</\1>", re.DOTALL | re.IGNORECASE)
BETWEEN_LINES = re.compile(r">\s*\n\s*<")
RUNS = re.compile(r"[ \t\r\n]+")
CSS_PUNCTUATION = re.compile(r"\s*([{}:;,>])\s*")
SVG_BETWEEN = re.compile(r">\s+<")
NODE_CHECK_TIMEOUT_S = 30


class MinifyError(Exception):
    pass


def html_text(text: str) -> str:
    if (not text.strip()) and ("\n" in text):
        return ""
    return RUNS.sub(" ", BETWEEN_LINES.sub("><", text))


def html(text: str) -> str:
    source = HTML_COMMENT.sub("", text)
    out: list[str] = []
    position = 0
    for match in HTML_KEEP.finditer(source):
        out.append(html_text(source[position : match.start()]))
        out.append(match.group(0))
        position = match.end()
    out.append(html_text(source[position:]))
    return BETWEEN_LINES.sub("><", "".join(out)).strip() + "\n"


def css_outside_strings(text: str) -> tuple[str, bool]:
    out: list[str] = []
    quoted = False
    index = 0
    while index < len(text):
        symbol = text[index]
        if symbol in "\"'":
            end = text.find(symbol, index + 1)
            if end < 0:
                raise MinifyError("a stylesheet has an unterminated string")
            out.append(text[index : end + 1])
            quoted = True
            index = end + 1
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            if end < 0:
                raise MinifyError("a stylesheet has an unterminated comment")
            index = end + 2
            continue
        out.append(symbol)
        index += 1
    return "".join(out), quoted


def css(text: str) -> str:
    stripped, quoted = css_outside_strings(text)
    spaced = RUNS.sub(" ", stripped)
    if quoted:
        return spaced.strip() + "\n"
    compact = CSS_PUNCTUATION.sub(r"\1", spaced)
    return compact.replace(";}", "}").strip() + "\n"


def javascript(text: str) -> str:
    if "`" in text or "/*" in text:
        return text
    lines = [line.strip() for line in text.splitlines()]
    return "\n".join(line for line in lines if line) + "\n"


def svg(text: str) -> str:
    without = HTML_COMMENT.sub("", text)
    return SVG_BETWEEN.sub("><", without).strip() + "\n"


def json_text(text: str) -> str:
    data = json.loads(text)
    return json.dumps(data, separators=(",", ":"), ensure_ascii=False) + "\n"


def check_javascript(name: str, text: str) -> None:
    node = shutil.which("node")
    if node is None:
        return
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder) / "check.js"
        path.write_text(text, encoding="utf-8")
        result = subprocess.run(
            [node, "--check", str(path)],
            check=False,
            capture_output=True,
            text=True,
            timeout=NODE_CHECK_TIMEOUT_S,
        )
    if result.returncode != 0:
        raise MinifyError(f"{name}: the minified script fails node --check")


def checked_javascript(name: str, text: str) -> str:
    small = javascript(text)
    check_javascript(name, small)
    return small


TEXT_MINIFIERS: dict[str, Callable[[str, str], str]] = {
    ".html": lambda _name, text: html(text),
    ".css": lambda _name, text: css(text),
    ".js": checked_javascript,
    ".svg": lambda _name, text: svg(text),
    ".json": lambda _name, text: json_text(text),
}


def minify(name: str, suffix: str, source: bytes) -> bytes:
    minifier = TEXT_MINIFIERS.get(suffix)
    if minifier is None:
        return source
    return minifier(name, source.decode("utf-8")).encode("utf-8")
