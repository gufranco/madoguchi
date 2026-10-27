# pyright: strict
# SPDX-FileCopyrightText: 2026 Gustavo Franco
# SPDX-License-Identifier: MIT

import gzip
import hashlib
import re
from dataclasses import dataclass
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit

from .minify import minify

SPEC_PATH = "/api/openapi.json"
ETAG_DIGITS = 16
FINGERPRINT_DIGITS = 8
IMMUTABLE = "public, max-age=31536000, immutable"
DEFAULT_DOCUMENT_MAX_AGE = 300
REWRITTEN = (".css", ".html")
RANKS = {".css": 2, ".html": 3}
URL_PATH = re.compile(r"^[A-Za-z0-9._/-]+$")
CONTENT_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".json": "application/json",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".ico": "image/vnd.microsoft.icon",
    ".txt": "text/plain; charset=utf-8",
}


class AssetError(Exception):
    pass


@dataclass(frozen=True, slots=True)
class Asset:
    url_paths: tuple[str, ...]
    content_type: str
    data: bytes
    etag: str
    policy: str
    cache_control: str


class PolicyScanner(HTMLParser):
    def __init__(self, name: str) -> None:
        super().__init__(convert_charrefs=True)
        self.name = name
        self.script_origins: list[str] = []
        self.style_origins: list[str] = []
        self.in_script = False

    def fail(self, reason: str) -> None:
        raise AssetError(f"{self.name}: {reason} is blocked by the page policy")

    def external(self, url: str, attributes: dict[str, str | None]) -> str:
        parts = urlsplit(url)
        if not parts.scheme:
            return ""
        if parts.scheme != "https":
            self.fail(f"a non-https resource {url}")
        if not attributes.get("integrity"):
            self.fail(f"{url} without an integrity hash")
        return f"https://{parts.netloc}"

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attributes = dict(attrs)
        if "style" in attributes:
            self.fail("a style attribute")
        if any(key.startswith("on") for key in attributes):
            self.fail("an inline event handler")
        if tag == "style":
            self.fail("an inline style element")
        if tag == "script":
            self.open_script(attributes)
        if tag == "link" and attributes.get("rel") == "stylesheet":
            origin = self.external(attributes.get("href") or "", attributes)
            self.style_origins.append(origin)

    def open_script(self, attributes: dict[str, str | None]) -> None:
        source = attributes.get("src")
        if not source:
            self.fail("an inline script")
        self.script_origins.append(self.external(source or "", attributes))
        self.in_script = True

    def handle_endtag(self, tag: str) -> None:
        if tag == "script":
            self.in_script = False

    def handle_data(self, data: str) -> None:
        if self.in_script and data.strip():
            self.fail("an inline script")


def sources(origins: list[str]) -> str:
    extra = sorted({origin for origin in origins if origin})
    return " ".join(["'self'", *extra])


def page_policy(name: str, text: str) -> str:
    scanner = PolicyScanner(name)
    scanner.feed(text)
    scanner.close()
    return "; ".join(
        [
            "default-src 'none'",
            f"script-src {sources(scanner.script_origins)}",
            f"style-src {sources(scanner.style_origins)}",
            "img-src 'self' data:",
            "connect-src 'self'",
            "font-src 'self'",
            "base-uri 'none'",
            "form-action 'self'",
            "frame-ancestors 'none'",
        ]
    )


def url_paths(relative: str) -> tuple[str, ...]:
    if relative == "index.html":
        return ("/", "/index.html")
    if relative.endswith("/index.html"):
        folder = relative[: -len("/index.html")]
        return (f"/{folder}", f"/{folder}/", f"/{relative}")
    return (f"/{relative}",)


def document_cache(max_age: int) -> str:
    return f"max-age={max_age}, must-revalidate" if max_age > 0 else "no-cache"


def make_asset(
    paths: tuple[str, ...],
    content_type: str,
    source: bytes,
    policy: str,
    cache_control: str = "no-cache",
) -> Asset:
    data = gzip.compress(source, compresslevel=9, mtime=0)
    etag = f'"{hashlib.sha256(source).hexdigest()[:ETAG_DIGITS]}"'
    return Asset(paths, content_type, data, etag, policy, cache_control)


def fingerprinted(relative: str, data: bytes) -> str:
    stem, dot, suffix = relative.rpartition(".")
    digest = hashlib.sha256(data).hexdigest()[:FINGERPRINT_DIGITS]
    return f"/{stem}.{digest}.{suffix}" if dot else f"/{relative}.{digest}"


def rewrite(text: str, renames: dict[str, str]) -> str:
    for old in sorted(renames, key=len, reverse=True):
        pattern = rf"(?<=[\"'(]){re.escape(old)}(?=[\"')?#])"
        text = re.sub(pattern, renames[old], text)
    return text


def checked_type(relative: str, suffix: str) -> str:
    content_type = CONTENT_TYPES.get(suffix)
    if content_type is None:
        raise AssetError(f"{relative}: no content type for this extension")
    if not URL_PATH.match(relative):
        raise AssetError(f"{relative}: use only letters, digits, '.', '_', '-' and '/'")
    return content_type


def site_asset(
    relative: str, source: bytes, renames: dict[str, str], max_age: int
) -> Asset:
    suffix = Path(relative).suffix.lower()
    content_type = checked_type(relative, suffix)
    small = minify(relative, suffix, source)
    if suffix in REWRITTEN:
        small = rewrite(small.decode("utf-8"), renames).encode("utf-8")
    if suffix == ".html":
        policy = page_policy(relative, small.decode("utf-8"))
        cache = document_cache(max_age)
        return make_asset(url_paths(relative), content_type, small, policy, cache)
    hashed = fingerprinted(relative, small)
    renames[f"/{relative}"] = hashed
    return make_asset((hashed,), content_type, small, "", IMMUTABLE)


def collect_assets(
    root: Path, spec: bytes, max_age: int = DEFAULT_DOCUMENT_MAX_AGE
) -> tuple[Asset, ...]:
    files = (
        sorted(path for path in root.rglob("*") if path.is_file())
        if root.is_dir()
        else []
    )
    relatives = [file.relative_to(root).as_posix() for file in files]
    ordered = sorted(
        relatives, key=lambda name: (RANKS.get(Path(name).suffix.lower(), 1), name)
    )
    renames: dict[str, str] = {}
    built = {
        name: site_asset(name, (root / name).read_bytes(), renames, max_age)
        for name in ordered
    }
    site = [built[name] for name in relatives]
    spec_asset = make_asset(
        (SPEC_PATH,), CONTENT_TYPES[".json"], spec, "", document_cache(max_age)
    )
    return (*site, spec_asset)
