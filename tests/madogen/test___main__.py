import base64
import hashlib
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_runs_as_a_module(tmp_path: Path) -> None:
    # Arrange
    file = tmp_path / "x.css"
    file.write_bytes(b"p{}")

    # Act
    completed = subprocess.run(
        [sys.executable, "-m", "tools.madogen", "sri", str(file)],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )

    # Assert
    digest = base64.b64encode(hashlib.sha384(b"p{}").digest()).decode()
    assert completed.returncode == 0
    assert completed.stdout.strip() == f"sha384-{digest}"


def test_exits_with_usage_errors(tmp_path: Path) -> None:
    # Arrange
    command = [sys.executable, "-m", "tools.madogen"]

    # Act
    completed = subprocess.run(
        command, cwd=ROOT, check=False, capture_output=True, text=True
    )

    # Assert
    assert completed.returncode == 2
    assert "usage: madogen" in completed.stderr
