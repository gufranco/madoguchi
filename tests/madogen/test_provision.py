import hashlib
import random
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.madogen.provision import (
    SecretsError,
    is_placeholder,
    make_secrets,
    placeholder,
    render_secrets,
)


def password(seed: int, length: int = 20) -> str:
    generator = random.Random(seed)
    alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#"
    return "".join(generator.choice(alphabet) for _ in range(length))


def byte_array(text: str, name: str) -> bytes:
    match = re.search(rf"{name}\[\d+\] PROGMEM = \{{([^}}]*)\}}", text)
    assert match is not None
    return bytes(int(value, 16) for value in match.group(1).replace(",", " ").split())


def test_derives_the_password_hash_with_pbkdf2() -> None:
    # Arrange
    generator = random.Random(11)
    secret = password(3)

    # Act
    secrets = make_secrets("admin", secret, 150, generator.randbytes)

    # Assert
    expected = hashlib.pbkdf2_hmac("sha256", secret.encode(), secrets.salt, 150)
    assert secrets.password_hash == expected
    assert len(secrets.key) == 32
    assert len(secrets.salt) == 16
    assert any(secrets.key)


def test_renders_every_value_into_flash_arrays() -> None:
    # Arrange
    generator = random.Random(5)
    secrets = make_secrets("admin", password(9), 200, generator.randbytes)

    # Act
    text = render_secrets(secrets)

    # Assert
    assert byte_array(text, "kTokenKey") == secrets.key
    assert byte_array(text, "kPasswordSalt") == secrets.salt
    assert byte_array(text, "kPasswordHash") == secrets.password_hash
    assert 'const char kUserName[] PROGMEM = "admin";' in text
    assert "constexpr uint32_t kPasswordIterations = 200UL;" in text
    assert "struct Provisioned {" in text
    assert (
        "  static const uint8_t* token_key() {\n    return &kTokenKey[0];\n  }" in text
    )


def test_placeholder_holds_an_all_zero_key() -> None:
    # Arrange
    text = render_secrets(placeholder())

    # Act
    unprovisioned = is_placeholder(text)

    # Assert
    assert unprovisioned
    assert byte_array(text, "kTokenKey") == bytes(32)


def test_provisioned_secrets_are_not_a_placeholder() -> None:
    # Arrange
    generator = random.Random(1)
    text = render_secrets(make_secrets("admin", password(2), 150, generator.randbytes))

    # Act
    unprovisioned = is_placeholder(text)

    # Assert
    assert not unprovisioned


@pytest.mark.parametrize(
    ("user", "secret", "iterations"),
    [
        ("admin", "short", 150),
        ("admin", "x" * 65, 150),
        ("ad min", password(4), 150),
        ("", password(4), 150),
        ("a" * 17, password(4), 150),
        ("admin", password(4), 0),
        ("admin", password(4), 1_000_001),
    ],
)
def test_rejects_unsafe_input(user: str, secret: str, iterations: int) -> None:
    # Arrange
    generator = random.Random(8)

    # Act
    with pytest.raises(SecretsError) as caught:
        make_secrets(user, secret, iterations, generator.randbytes)

    # Assert
    assert str(caught.value)
