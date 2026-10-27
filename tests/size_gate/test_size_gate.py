import importlib.util
import sys
from pathlib import Path
from types import ModuleType
from typing import Any

TOOL = Path(__file__).resolve().parents[2] / "tools" / "size_gate.py"
ROOT = "/work/Madoguchi"


def load_tool() -> ModuleType:
    spec = importlib.util.spec_from_file_location("size_gate", TOOL)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules["size_gate"] = module
    spec.loader.exec_module(module)
    return module


def report(
    text: int, data: int, errors: str = "", success: bool = True
) -> dict[str, Any]:
    return {
        "success": success,
        "compiler_err": errors,
        "builder_result": {
            "executable_sections_size": [
                {"name": "text", "size": text, "max_size": 32256},
                {"name": "data", "size": data, "max_size": 2048},
            ]
        },
    }


def test_accepts_a_build_inside_every_budget() -> None:
    # Arrange
    tool = load_tool()

    # Act
    problems = tool.evaluate(report(16504, 509), min_free_ram=1024, own_root=ROOT)

    # Assert
    assert problems == []


def test_rejects_flash_over_the_limit() -> None:
    # Arrange
    tool = load_tool()

    # Act
    problems = tool.evaluate(report(32257, 509), min_free_ram=0, own_root=ROOT)

    # Assert
    assert problems == ["text uses 32257 of 32256 bytes"]


def test_rejects_too_little_free_ram() -> None:
    # Arrange
    tool = load_tool()

    # Act
    problems = tool.evaluate(report(16504, 1100), min_free_ram=1024, own_root=ROOT)

    # Assert
    assert problems == ["data leaves 948 bytes free, needs 1024"]


def test_rejects_warnings_from_our_sources_only() -> None:
    # Arrange
    tool = load_tool()
    errors = (
        "/home/runner/.arduino15/cores/arduino/new.cpp:59:60: warning: unused parameter\n"
        f"{ROOT}/src/mado/Exchange.h:10:3: warning: unused variable 'x'\n"
    )

    # Act
    problems = tool.evaluate(report(16504, 509, errors), min_free_ram=0, own_root=ROOT)

    # Assert
    assert problems == [
        f"{ROOT}/src/mado/Exchange.h:10:3: warning: unused variable 'x'"
    ]


def test_rejects_a_failed_build() -> None:
    # Arrange
    tool = load_tool()

    # Act
    problems = tool.evaluate(
        report(0, 0, "fatal error", success=False), min_free_ram=0, own_root=ROOT
    )

    # Assert
    assert problems == ["build failed"]


def test_rejects_a_build_without_a_size_report() -> None:
    # Arrange
    tool = load_tool()
    empty = {"success": True, "compiler_err": "", "builder_result": {}}

    # Act
    problems = tool.evaluate(empty, min_free_ram=0, own_root=ROOT)

    # Assert
    assert problems == ["no section sizes in the build report"]
