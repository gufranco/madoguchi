import shutil
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.madogen.minify import (
    MinifyError,
    css,
    html,
    javascript,
    json_text,
    minify,
    svg,
)


def test_html_drops_comments_and_whitespace_between_lines() -> None:
    # Arrange
    source = "<main>\n  <!-- note -->\n  <h1>Files  on  the card</h1>\n  <p>a <b>b</b></p>\n</main>\n"

    # Act
    small = html(source)

    # Assert
    assert small == "<main><h1>Files on the card</h1><p>a <b>b</b></p></main>\n"


def test_html_keeps_preformatted_text_as_written() -> None:
    # Arrange
    source = "<div>\n<pre>a\n   b</pre>\n<textarea>x\n  y</textarea>\n</div>\n"

    # Act
    small = html(source)

    # Assert
    assert "<pre>a\n   b</pre>" in small
    assert "<textarea>x\n  y</textarea>" in small


def test_css_drops_comments_and_spaces_around_punctuation() -> None:
    # Arrange
    source = "/* theme */\nbody {\n  margin: 0;\n  color: red;\n}\na > b, c { x: y; }\n"

    # Act
    small = css(source)

    # Assert
    assert small == "body{margin:0;color:red}a>b,c{x:y}\n"


def test_css_with_strings_only_collapses_whitespace() -> None:
    # Arrange
    source = 'a::after {\n  content: "x : y";\n}\n'

    # Act
    small = css(source)

    # Assert
    assert small == 'a::after { content: "x : y"; }\n'


def test_css_refuses_an_unterminated_comment() -> None:
    # Arrange
    source = "a { b: c; } /* open"

    # Act
    with pytest.raises(MinifyError) as caught:
        css(source)

    # Assert
    assert "unterminated comment" in str(caught.value)


def test_javascript_strips_indentation_and_blank_lines_only() -> None:
    # Arrange
    source = '"use strict";\n\nfunction f() {\n    return 1;\n}\n'

    # Act
    small = javascript(source)

    # Assert
    assert small == '"use strict";\nfunction f() {\nreturn 1;\n}\n'


def test_javascript_with_a_template_literal_is_left_alone() -> None:
    # Arrange
    source = "const text = `a\n    b`;\n"

    # Act
    small = javascript(source)

    # Assert
    assert small == source


def test_svg_and_json_become_compact() -> None:
    # Arrange
    image = "<svg>\n  <!-- x -->\n  <rect/>\n</svg>\n"
    document = '{\n  "a": [1, 2],\n  "b": "c"\n}\n'

    # Act
    small_image = svg(image)

    # Assert
    assert small_image == "<svg><rect/></svg>\n"
    assert json_text(document) == '{"a":[1,2],"b":"c"}\n'


@pytest.mark.skipif(shutil.which("node") is None, reason="node is not installed")
def test_minify_refuses_a_script_node_cannot_parse() -> None:
    # Arrange
    source = b"function (\n"

    # Act
    with pytest.raises(MinifyError) as caught:
        minify("bad.js", ".js", source)

    # Assert
    assert "node --check" in str(caught.value)


def test_minify_passes_binary_files_through() -> None:
    # Arrange
    source = b"\x89PNG\r\n\x1a\n"

    # Act
    small = minify("logo.png", ".png", source)

    # Assert
    assert small == source
