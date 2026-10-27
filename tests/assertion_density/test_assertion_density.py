import importlib.util
import sys
from pathlib import Path
from types import ModuleType

import pytest

TOOL = Path(__file__).resolve().parents[2] / "tools" / "assertion_density.py"


def load_tool() -> ModuleType:
    spec = importlib.util.spec_from_file_location("assertion_density", TOOL)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules["assertion_density"] = module
    spec.loader.exec_module(module)
    return module


def write_header(root: Path, name: str, body: str) -> Path:
    path = root / name
    path.write_text(body, encoding="utf-8")
    return path


def test_counts_checks_and_static_asserts_but_not_the_definition(
    tmp_path: Path,
) -> None:
    # Arrange
    tool = load_tool()
    header = write_header(
        tmp_path,
        "check.h",
        "inline bool check(const bool c) { return c; }\n"
        'static_assert(true, "x");\n'
        "inline int f(int v) { const bool ok = check(v > 0); return ok ? v : 0; }\n",
    )

    # Act
    assertions = tool.count_assertions(header)

    # Assert
    assert assertions == 2


def test_measures_functions_per_header(tmp_path: Path) -> None:
    # Arrange
    tool = load_tool()
    write_header(
        tmp_path,
        "a.h",
        "inline int one(int v) {\n  return check(v > 0) ? v : 0;\n}\n"
        "inline int two(int v) {\n  return v;\n}\n",
    )

    # Act
    densities = tool.measure(tmp_path)

    # Assert
    assert [
        (item.path.name, item.functions, item.assertions) for item in densities
    ] == [("a.h", 2, 1)]


def test_main_fails_below_the_minimum(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    # Arrange
    tool = load_tool()
    write_header(tmp_path, "a.h", "inline int one(int v) {\n  return v;\n}\n")
    monkeypatch.setattr(sys, "argv", ["assertion_density", "--root", str(tmp_path)])

    # Act
    status = tool.main()

    # Assert
    assert status == 1
    assert (
        "average 0.00 assertions per function, minimum 2.00" in capsys.readouterr().out
    )


def test_main_passes_at_the_minimum(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # Arrange
    tool = load_tool()
    write_header(
        tmp_path,
        "a.h",
        "inline int one(int v) {\n  const bool a = check(v > 0);\n"
        "  const bool b = check(v < 9);\n  return a && b ? v : 0;\n}\n",
    )
    monkeypatch.setattr(sys, "argv", ["assertion_density", "--root", str(tmp_path)])

    # Act
    status = tool.main()

    # Assert
    assert status == 0
