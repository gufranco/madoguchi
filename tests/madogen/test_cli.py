import base64
import hashlib
import io
import json
import random
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.madogen.cli import main
from tools.madogen.provision import is_placeholder


def spec() -> dict[str, object]:
    return {
        "openapi": "3.1.0",
        "info": {"title": "Ping", "version": "1.0.0"},
        "components": {
            "securitySchemes": {
                "bearerAuth": {
                    "type": "http",
                    "scheme": "bearer",
                    "bearerFormat": "JWT",
                }
            }
        },
        "paths": {
            "/api/ping": {
                "get": {"operationId": "ping", "responses": {}},
                "delete": {
                    "operationId": "reset",
                    "security": [{"bearerAuth": []}],
                    "responses": {},
                },
            }
        },
    }


def sketch(root: Path) -> Path:
    folder = root / "Ping"
    (folder / "www").mkdir(parents=True)
    (folder / "openapi.json").write_text(json.dumps(spec(), indent=2), encoding="utf-8")
    (folder / "www" / "index.html").write_text(
        "<!doctype html><title>p</title>", encoding="utf-8"
    )
    return folder


def password(seed: int) -> str:
    generator = random.Random(seed)
    return "".join(
        generator.choice("abcdefghijklmnopqrstuvwxyz0123456789") for _ in range(20)
    )


def test_generate_writes_the_header_and_a_placeholder(tmp_path: Path) -> None:
    # Arrange
    folder = sketch(tmp_path)

    # Act
    code = main(["generate", str(folder)])

    # Assert
    assert code == 0
    header = (folder / "generated_api.h").read_text(encoding="utf-8")
    assert "kReset" in header
    assert "namespace ping {" in header
    assert is_placeholder((folder / "arduino_secrets.h").read_text(encoding="utf-8"))


def test_generate_skips_secrets_for_a_public_api(tmp_path: Path) -> None:
    # Arrange
    folder = sketch(tmp_path)
    document = spec()
    paths = document["paths"]
    assert isinstance(paths, dict)
    del paths["/api/ping"]["delete"]
    (folder / "openapi.json").write_text(json.dumps(document), encoding="utf-8")

    # Act
    code = main(["generate", str(folder)])

    # Assert
    assert code == 0
    assert not (folder / "arduino_secrets.h").exists()


def test_generate_keeps_existing_secrets(tmp_path: Path) -> None:
    # Arrange
    folder = sketch(tmp_path)
    (folder / "arduino_secrets.h").write_text("kept", encoding="utf-8")

    # Act
    code = main(["generate", str(folder)])

    # Assert
    assert code == 0
    assert (folder / "arduino_secrets.h").read_text(encoding="utf-8") == "kept"


def test_check_passes_on_fresh_output(tmp_path: Path) -> None:
    # Arrange
    folder = sketch(tmp_path)
    main(["generate", str(folder)])

    # Act
    code = main(["check", str(folder)])

    # Assert
    assert code == 0


def test_check_names_a_stale_header(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    # Arrange
    folder = sketch(tmp_path)
    main(["generate", str(folder)])
    (folder / "www" / "index.html").write_text(
        "<!doctype html><title>q</title>", encoding="utf-8"
    )

    # Act
    code = main(["check", str(folder)])

    # Assert
    assert code == 1
    assert "generated_api.h is stale" in capsys.readouterr().out


def test_check_refuses_provisioned_secrets(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    # Arrange
    folder = sketch(tmp_path)
    main(["generate", str(folder)])
    monkeypatch.setattr(sys, "stdin", io.StringIO(password(4) + "\n"))
    main(["secrets", str(folder), "--user", "admin", "--password-stdin"])

    # Act
    code = main(["check", str(folder)])

    # Assert
    assert code == 1
    assert "holds a real key" in capsys.readouterr().out


def test_check_reports_a_missing_header(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    # Arrange
    folder = sketch(tmp_path)

    # Act
    code = main(["check", str(folder)])

    # Assert
    assert code == 1
    assert "generated_api.h is missing" in capsys.readouterr().out


def test_secrets_reads_the_password_from_stdin(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # Arrange
    folder = sketch(tmp_path)
    monkeypatch.setattr(sys, "stdin", io.StringIO(password(6) + "\n"))

    # Act
    code = main(["secrets", str(folder), "--user", "admin", "--password-stdin"])

    # Assert
    assert code == 0
    assert not is_placeholder(
        (folder / "arduino_secrets.h").read_text(encoding="utf-8")
    )


def test_secrets_rejects_a_short_password(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    # Arrange
    folder = sketch(tmp_path)
    monkeypatch.setattr(sys, "stdin", io.StringIO("short\n"))

    # Act
    code = main(["secrets", str(folder), "--user", "admin", "--password-stdin"])

    # Assert
    assert code == 2
    assert "12 to 64" in capsys.readouterr().err


def test_generate_reports_an_invalid_spec(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    # Arrange
    folder = sketch(tmp_path)
    document = spec()
    document["openapi"] = "3.0.0"
    (folder / "openapi.json").write_text(json.dumps(document), encoding="utf-8")

    # Act
    code = main(["generate", str(folder)])

    # Assert
    assert code == 2
    assert "/openapi: must be 3.1.x" in capsys.readouterr().err


def test_sri_prints_a_sha384_integrity_value(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    # Arrange
    file = tmp_path / "x.js"
    file.write_bytes(b"console.log(1)")

    # Act
    code = main(["sri", str(file)])

    # Assert
    digest = base64.b64encode(hashlib.sha384(b"console.log(1)").digest()).decode()
    assert code == 0
    assert capsys.readouterr().out.strip() == f"sha384-{digest}"
