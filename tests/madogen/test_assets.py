import gzip
import hashlib
import random
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.madogen.assets import AssetError, collect_assets


def write(root: Path, relative: str, text: str) -> None:
    target = root / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def page(body: str) -> str:
    return f'<!doctype html><html lang="en"><head><title>t</title></head><body>{body}</body></html>'


def random_word(seed: int) -> str:
    generator = random.Random(seed)
    return "".join(generator.choice("abcdefghijklmnopqrstuvwxyz") for _ in range(12))


def test_maps_index_files_to_their_directory(tmp_path: Path) -> None:
    # Arrange
    write(tmp_path, "index.html", page("home"))
    write(tmp_path, "docs/index.html", page("docs"))

    # Act
    assets = collect_assets(tmp_path, b"{}")

    # Assert
    paths = [asset.url_paths for asset in assets]
    assert ("/", "/index.html") in paths
    assert ("/docs", "/docs/", "/docs/index.html") in paths
    assert ("/api/openapi.json",) in paths


def test_compresses_deterministically_and_fingerprints_the_name(tmp_path: Path) -> None:
    # Arrange
    text = f"console.log('{random_word(7)}');\n"
    write(tmp_path, "app.js", text)
    digest = hashlib.sha256(text.encode()).hexdigest()

    # Act
    first = collect_assets(tmp_path, b"{}")
    second = collect_assets(tmp_path, b"{}")

    # Assert
    script = first[0]
    assert script.url_paths == (f"/app.{digest[:8]}.js",)
    assert gzip.decompress(script.data).decode() == text
    assert script.data == second[0].data
    assert script.etag == f'"{digest[:16]}"'
    assert script.content_type == "text/javascript; charset=utf-8"
    assert script.cache_control == "public, max-age=31536000, immutable"
    assert script.policy == ""


def test_minifies_and_points_pages_at_fingerprinted_files(tmp_path: Path) -> None:
    # Arrange
    write(tmp_path, "style.css", "body {\n  margin: 0;\n}\n")
    write(tmp_path, "app.js", "  run();\n")
    write(
        tmp_path,
        "index.html",
        page(
            '<link rel="stylesheet" href="/style.css">\n<script src="/app.js"></script>'
        ),
    )

    # Act
    assets = collect_assets(tmp_path, b"{}", 120)

    # Assert
    by_type = {asset.content_type.split(";")[0]: asset for asset in assets}
    style = by_type["text/css"]
    script = by_type["text/javascript"]
    home = by_type["text/html"]
    markup = gzip.decompress(home.data).decode()
    assert gzip.decompress(style.data).decode() == "body{margin:0}\n"
    assert f'href="{style.url_paths[0]}"' in markup
    assert f'src="{script.url_paths[0]}"' in markup
    assert home.url_paths == ("/", "/index.html")
    assert home.cache_control == "max-age=120, must-revalidate"
    assert by_type["application/json"].cache_control == "max-age=120, must-revalidate"


def test_asks_entry_documents_to_revalidate_when_the_age_is_zero(
    tmp_path: Path,
) -> None:
    # Arrange
    write(tmp_path, "index.html", page("home"))

    # Act
    assets = collect_assets(tmp_path, b"{}", 0)

    # Assert
    assert assets[0].cache_control == "no-cache"


def test_builds_a_policy_that_allows_pinned_cdn_files(tmp_path: Path) -> None:
    # Arrange
    script = (
        '<script src="https://unpkg.com/x@1.0.0/x.js" integrity="sha384-abc" '
        'crossorigin="anonymous"></script>'
    )
    style = (
        '<link rel="stylesheet" href="https://unpkg.com/x@1.0.0/x.css" '
        'integrity="sha384-def" crossorigin="anonymous">'
    )
    write(tmp_path, "index.html", page(style + script))

    # Act
    assets = collect_assets(tmp_path, b"{}")

    # Assert
    html = next(asset for asset in assets if asset.url_paths[0] == "/")
    assert html.policy == (
        "default-src 'none'; script-src 'self' https://unpkg.com; "
        "style-src 'self' https://unpkg.com; img-src 'self' data:; connect-src 'self'; "
        "font-src 'self'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'"
    )


@pytest.mark.parametrize(
    ("body", "message"),
    [
        ("<script>alert(1)</script>", "inline script"),
        ('<div style="color:red"></div>', "style attribute"),
        ("<style>p{}</style>", "inline style"),
        ('<button onclick="go()"></button>', "event handler"),
        ('<script src="https://unpkg.com/x.js"></script>', "integrity"),
        (
            '<script src="http://example.com/x.js" integrity="sha384-a"></script>',
            "https",
        ),
    ],
)
def test_rejects_markup_the_policy_would_block(
    tmp_path: Path, body: str, message: str
) -> None:
    # Arrange
    write(tmp_path, "index.html", page(body))

    # Act
    with pytest.raises(AssetError) as caught:
        collect_assets(tmp_path, b"{}")

    # Assert
    assert message in str(caught.value)


def test_rejects_unknown_file_types(tmp_path: Path) -> None:
    # Arrange
    write(tmp_path, "notes.docx", "x")

    # Act
    with pytest.raises(AssetError) as caught:
        collect_assets(tmp_path, b"{}")

    # Assert
    assert "notes.docx" in str(caught.value)


def test_rejects_paths_the_server_cannot_serve(tmp_path: Path) -> None:
    # Arrange
    write(tmp_path, "My Page.html", page("x"))

    # Act
    with pytest.raises(AssetError) as caught:
        collect_assets(tmp_path, b"{}")

    # Assert
    assert "My Page.html" in str(caught.value)


def test_works_without_a_site_folder(tmp_path: Path) -> None:
    # Arrange
    missing = tmp_path / "www"

    # Act
    assets = collect_assets(missing, b'{"openapi":"3.1.0"}')

    # Assert
    assert [asset.url_paths for asset in assets] == [("/api/openapi.json",)]
    assert gzip.decompress(assets[0].data) == b'{"openapi":"3.1.0"}'
