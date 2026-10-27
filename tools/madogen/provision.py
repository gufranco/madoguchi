# pyright: strict
# SPDX-FileCopyrightText: 2026 Gustavo Franco
# SPDX-License-Identifier: MIT

import hashlib
import re
from collections.abc import Callable
from dataclasses import dataclass

KEY_BYTES = 32
SALT_BYTES = 16
HASH_BYTES = 32
MIN_PASSWORD = 12
MAX_PASSWORD = 64
MAX_ITERATIONS = 1_000_000
DEFAULT_ITERATIONS = 150
SUBJECT = re.compile(r"^[A-Za-z0-9._-]{1,16}$")
KEY_ARRAY = re.compile(r"kTokenKey\[32\] PROGMEM = \{([^}]*)\}")


ACCESSORS = (
    "struct Provisioned {\n"
    "  static const uint8_t* token_key() {\n    return &kTokenKey[0];\n  }\n\n"
    "  static const char* user_name() {\n    return &kUserName[0];\n  }\n\n"
    "  static const uint8_t* password_salt() {\n    return &kPasswordSalt[0];\n  }\n\n"
    "  static const uint8_t* password_hash() {\n    return &kPasswordHash[0];\n  }\n\n"
    "  static uint32_t password_iterations() {\n    return kPasswordIterations;\n  }\n"
    "};\n\n"
)


class SecretsError(Exception):
    pass


@dataclass(frozen=True, slots=True)
class Secrets:
    user: str
    key: bytes
    salt: bytes
    password_hash: bytes
    iterations: int


def make_secrets(
    user: str, password: str, iterations: int, random_bytes: Callable[[int], bytes]
) -> Secrets:
    if not SUBJECT.match(user):
        raise SecretsError(
            "the user name takes 1 to 16 letters, digits, '.', '_' or '-'"
        )
    if not MIN_PASSWORD <= len(password) <= MAX_PASSWORD:
        raise SecretsError(
            f"the password must be {MIN_PASSWORD} to {MAX_PASSWORD} characters"
        )
    if not 1 <= iterations <= MAX_ITERATIONS:
        raise SecretsError(f"iterations must be 1 to {MAX_ITERATIONS}")
    key = random_bytes(KEY_BYTES)
    salt = random_bytes(SALT_BYTES)
    derived = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, iterations)
    return Secrets(user, key, salt, derived, iterations)


def placeholder() -> Secrets:
    return Secrets(
        "admin",
        bytes(KEY_BYTES),
        bytes(SALT_BYTES),
        bytes(HASH_BYTES),
        DEFAULT_ITERATIONS,
    )


def byte_rows(data: bytes) -> str:
    rows = [data[start : start + 8] for start in range(0, len(data), 8)]
    lines = [", ".join(f"0x{value:02x}" for value in row) for row in rows]
    return ",\n    ".join(lines)


def render_secrets(secrets: Secrets) -> str:
    return (
        "// SPDX-FileCopyrightText: 2026 Gustavo Franco\n"
        "// SPDX-License-Identifier: MIT\n\n"
        "#ifndef ARDUINO_SECRETS_H\n#define ARDUINO_SECRETS_H\n\n"
        "#include <Arduino.h>\n#include <stdint.h>\n\nnamespace secrets {\n\n"
        f"const uint8_t kTokenKey[{KEY_BYTES}] PROGMEM = {{\n    {byte_rows(secrets.key)}}};\n"
        f'const char kUserName[] PROGMEM = "{secrets.user}";\n'
        f"const uint8_t kPasswordSalt[{SALT_BYTES}] PROGMEM = {{\n    {byte_rows(secrets.salt)}}};\n"
        f"const uint8_t kPasswordHash[{HASH_BYTES}] PROGMEM = {{\n"
        f"    {byte_rows(secrets.password_hash)}}};\n"
        f"constexpr uint32_t kPasswordIterations = {secrets.iterations}UL;\n\n"
        f"{ACCESSORS}"
        "}\n\n#endif\n"
    )


def is_placeholder(text: str) -> bool:
    match = KEY_ARRAY.search(text)
    if match is None:
        raise SecretsError("no kTokenKey array of 32 bytes found")
    values = [int(token, 16) for token in match.group(1).replace(",", " ").split()]
    return len(values) == KEY_BYTES and not any(values)
