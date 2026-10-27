# pyright: strict
# SPDX-FileCopyrightText: 2026 Gustavo Franco
# SPDX-License-Identifier: MIT

import re
from dataclasses import dataclass
from enum import Enum
from typing import cast

MAX_OPERATIONS = 31
MAX_PROPERTIES = 8
MAX_STRING_LENGTH = 200
INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1
METHODS = ("get", "post", "put", "patch", "delete")
BEARER_SCHEME = "bearerAuth"
IDENTIFIER = re.compile(r"^[a-z][a-z0-9_]*$")
OPERATION_ID = re.compile(r"^[a-z][A-Za-z0-9]*$")
HEADER_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9-]{0,30}$")
DEFAULT_BUFFER_SIZE = 32
MIN_OUTPUT_BUFFER = 8
MAX_OUTPUT_BUFFER = 1460
MAX_FILE_CHUNK = 1024
DEFAULT_DOCUMENT_MAX_AGE = 300
MAX_DOCUMENT_MAX_AGE = 86400
CAPTURED_HEADER_LIMIT = 8
BASE_HEADERS = ("Authorization", "Content-Type", "Accept-Encoding", "If-None-Match")
SCHEMA_NAME = re.compile(r"^[A-Z][A-Za-z0-9]*$")
PATH_SEGMENT = re.compile(r"^[a-z0-9._-]+$")
PARAMETER_SEGMENT = re.compile(r"^\{[a-z][a-z0-9_]*\}$")
SCHEMA_REF = re.compile(r"^#/components/schemas/([A-Za-z0-9]+)$")

Json = dict[str, object]


class SpecError(Exception):
    def __init__(self, pointer: str, message: str) -> None:
        super().__init__(f"{pointer or '/'}: {message}")
        self.pointer = pointer


class Kind(Enum):
    STRING = "string"
    INTEGER = "integer"
    BOOLEAN = "boolean"


@dataclass(frozen=True, slots=True)
class Field:
    name: str
    kind: Kind
    max_length: int
    minimum: int
    maximum: int
    required: bool
    read_only: bool


@dataclass(frozen=True, slots=True)
class Schema:
    name: str
    fields: tuple[Field, ...]
    stored: bool = False


@dataclass(frozen=True, slots=True)
class Operation:
    operation_id: str
    method: str
    path: str
    route_pattern: str
    id_prefix: str
    secured: bool
    request_schema: str
    response_schemas: tuple[str, ...] = ()
    header_parameters: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class Buffers:
    output: int = DEFAULT_BUFFER_SIZE
    file_chunk: int = DEFAULT_BUFFER_SIZE
    document_max_age: int = DEFAULT_DOCUMENT_MAX_AGE


@dataclass(frozen=True, slots=True)
class Api:
    title: str
    schemas: tuple[Schema, ...]
    operations: tuple[Operation, ...]
    buffers: Buffers = Buffers()


def escape(name: str) -> str:
    return name.replace("~", "~0").replace("/", "~1")


def child(node: Json, key: str, pointer: str) -> object:
    if key not in node:
        raise SpecError(pointer, f"missing '{key}'")
    return node[key]


def as_object(value: object, pointer: str) -> Json:
    if not isinstance(value, dict):
        raise SpecError(pointer, "must be an object")
    return cast(Json, value)


def as_list(value: object, pointer: str) -> list[object]:
    if not isinstance(value, list):
        raise SpecError(pointer, "must be a list")
    return cast(list[object], value)


def as_int(value: object, pointer: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise SpecError(pointer, "must be an integer")
    return value


def string_limits(node: Json, pointer: str) -> tuple[int, int, int]:
    length = as_int(child(node, "maxLength", pointer), f"{pointer}/maxLength")
    if not 1 <= length <= MAX_STRING_LENGTH:
        raise SpecError(f"{pointer}/maxLength", f"must be 1 to {MAX_STRING_LENGTH}")
    return length, 0, 0


def integer_limits(node: Json, pointer: str) -> tuple[int, int, int]:
    low = as_int(child(node, "minimum", pointer), f"{pointer}/minimum")
    high = as_int(child(node, "maximum", pointer), f"{pointer}/maximum")
    if not INT32_MIN <= low <= high <= INT32_MAX:
        raise SpecError(pointer, "minimum and maximum must be ordered int32 values")
    return 0, low, high


def parse_field(name: str, node: Json, required: bool, pointer: str) -> Field:
    if not IDENTIFIER.match(name):
        raise SpecError(pointer, "property names must be lowercase identifiers")
    kind_name = child(node, "type", pointer)
    if kind_name not in {kind.value for kind in Kind}:
        raise SpecError(f"{pointer}/type", "must be string, integer or boolean")
    kind = Kind(kind_name)
    limits = (0, 0, 0)
    if kind is Kind.STRING:
        limits = string_limits(node, pointer)
    if kind is Kind.INTEGER:
        limits = integer_limits(node, pointer)
    read_only = node.get("readOnly", False) is True
    return Field(name, kind, limits[0], limits[1], limits[2], required, read_only)


def parse_schema(name: str, node: Json, pointer: str) -> Schema:
    if not SCHEMA_NAME.match(name):
        raise SpecError(pointer, "schema names must be PascalCase")
    if node.get("type") != "object" or node.get("additionalProperties") is not False:
        raise SpecError(pointer, "must be an object with additionalProperties false")
    properties = as_object(child(node, "properties", pointer), f"{pointer}/properties")
    names = {
        str(entry) for entry in as_list(node.get("required", []), f"{pointer}/required")
    }
    fields = tuple(
        parse_field(
            key,
            as_object(value, f"{pointer}/properties/{escape(key)}"),
            key in names,
            f"{pointer}/properties/{escape(key)}",
        )
        for key, value in properties.items()
    )
    if not 1 <= len(fields) <= MAX_PROPERTIES:
        raise SpecError(
            f"{pointer}/properties", f"must hold 1 to {MAX_PROPERTIES} properties"
        )
    if names - {field.name for field in fields}:
        raise SpecError(f"{pointer}/required", "names a property that does not exist")
    stored = node.get("x-mado-writer", False)
    if not isinstance(stored, bool):
        raise SpecError(f"{pointer}/x-mado-writer", "must be true or false")
    return Schema(name, fields, stored)


def parse_route(path: str, pointer: str) -> tuple[str, str]:
    if not path.startswith("/api/"):
        raise SpecError(pointer, "API paths must start with /api/")
    segments = path.split("/")[1:]
    for index, segment in enumerate(segments):
        if PARAMETER_SEGMENT.match(segment) and index == len(segments) - 1:
            prefix = path[: len(path) - len(segment)]
            return f"{prefix}*", prefix
        if not PATH_SEGMENT.match(segment):
            raise SpecError(pointer, "only one trailing {id} parameter is supported")
    return path, ""


def parse_security(value: object, pointer: str) -> bool:
    requirements = as_list(value, pointer)
    if not requirements:
        return False
    if requirements != [{BEARER_SCHEME: []}]:
        raise SpecError(pointer, f"only [] or [{{'{BEARER_SCHEME}': []}}] is supported")
    return True


def parse_request_schema(node: Json, schemas: set[str], pointer: str) -> str:
    if "requestBody" not in node:
        return ""
    body = as_object(node["requestBody"], f"{pointer}/requestBody")
    content = as_object(
        child(body, "content", f"{pointer}/requestBody"),
        f"{pointer}/requestBody/content",
    )
    if list(content) == ["application/octet-stream"]:
        return ""
    if list(content) != ["application/json"]:
        raise SpecError(
            f"{pointer}/requestBody/content",
            "must hold only application/json or application/octet-stream",
        )
    media = as_object(
        content["application/json"], f"{pointer}/requestBody/content/application~1json"
    )
    schema = as_object(
        child(media, "schema", pointer),
        f"{pointer}/requestBody/content/application~1json/schema",
    )
    match = SCHEMA_REF.match(str(schema.get("$ref", "")))
    if match is None or match.group(1) not in schemas:
        raise SpecError(f"{pointer}/requestBody", "must reference a component schema")
    return match.group(1)


def response_schema(response: object, schemas: set[str]) -> str:
    content = (
        cast(Json, response).get("content") if isinstance(response, dict) else None
    )
    media = (
        cast(Json, content).get("application/json")
        if isinstance(content, dict)
        else None
    )
    schema = cast(Json, media).get("schema") if isinstance(media, dict) else None
    if not isinstance(schema, dict):
        return ""
    node = cast(Json, schema)
    items = node.get("items")
    target = (
        cast(Json, items)
        if node.get("type") == "array" and isinstance(items, dict)
        else node
    )
    match = SCHEMA_REF.match(str(target.get("$ref", "")))
    return match.group(1) if match is not None and match.group(1) in schemas else ""


def parse_response_schemas(
    node: Json, schemas: set[str], pointer: str
) -> tuple[str, ...]:
    responses = as_object(node.get("responses", {}), f"{pointer}/responses")
    names = [response_schema(response, schemas) for response in responses.values()]
    return tuple(sorted({name for name in names if name}))


def parse_header_parameters(node: Json, pointer: str) -> tuple[str, ...]:
    names: list[str] = []
    for index, value in enumerate(
        as_list(node.get("parameters", []), f"{pointer}/parameters")
    ):
        parameter = as_object(value, f"{pointer}/parameters/{index}")
        if parameter.get("in") != "header":
            continue
        name = str(child(parameter, "name", f"{pointer}/parameters/{index}"))
        if not HEADER_NAME.match(name):
            raise SpecError(
                f"{pointer}/parameters/{index}/name", "must be a header token"
            )
        names.append(name)
    return tuple(names)


def parse_operation(
    path: str, method: str, node: Json, context: tuple[bool, set[str]]
) -> Operation:
    pointer = f"/paths/{escape(path)}/{method}"
    operation_id = str(child(node, "operationId", pointer))
    if not OPERATION_ID.match(operation_id):
        raise SpecError(f"{pointer}/operationId", "must be a camelCase identifier")
    secured = context[0]
    if "security" in node:
        secured = parse_security(node["security"], f"{pointer}/security")
    pattern, prefix = parse_route(path, f"/paths/{escape(path)}")
    request_schema = parse_request_schema(node, context[1], pointer)
    responses = parse_response_schemas(node, context[1], pointer)
    headers = parse_header_parameters(node, pointer)
    return Operation(
        operation_id,
        method,
        path,
        pattern,
        prefix,
        secured,
        request_schema,
        responses,
        headers,
    )


def parse_schemes(components: Json) -> None:
    pointer = "/components/securitySchemes"
    schemes = as_object(components.get("securitySchemes", {}), pointer)
    for name, value in schemes.items():
        scheme = as_object(value, f"{pointer}/{escape(name)}")
        expected = {"type": "http", "scheme": "bearer", "bearerFormat": "JWT"}
        if name != BEARER_SCHEME or scheme != expected:
            raise SpecError(
                f"{pointer}/{escape(name)}",
                f"only {BEARER_SCHEME} as an HTTP bearer JWT is supported",
            )


def check_header_parameters(operations: list[Operation]) -> None:
    extras = {
        name
        for operation in operations
        for name in operation.header_parameters
        if name not in BASE_HEADERS
    }
    room = CAPTURED_HEADER_LIMIT - len(BASE_HEADERS)
    if len(extras) > room:
        raise SpecError("/paths", f"must declare at most {room} header parameters")


def parse_operations(
    paths: Json, context: tuple[bool, set[str]]
) -> tuple[Operation, ...]:
    operations: list[Operation] = []
    seen: set[str] = set()
    for path, value in paths.items():
        item = as_object(value, f"/paths/{escape(path)}")
        parse_route(path, f"/paths/{escape(path)}")
        for method in METHODS:
            if method not in item:
                continue
            operation = parse_operation(
                path,
                method,
                as_object(item[method], f"/paths/{escape(path)}/{method}"),
                context,
            )
            if operation.operation_id in seen:
                raise SpecError(
                    f"/paths/{escape(path)}/{method}/operationId", "is not unique"
                )
            seen.add(operation.operation_id)
            operations.append(operation)
    if len(operations) > MAX_OPERATIONS:
        raise SpecError("/paths", f"must hold at most {MAX_OPERATIONS} operations")
    check_header_parameters(operations)
    return tuple(operations)


def buffer_size(node: Json, key: str, bounds: tuple[int, int]) -> int:
    pointer = f"/x-mado-limits/{key}"
    value = as_int(node.get(key, DEFAULT_BUFFER_SIZE), pointer)
    if not bounds[0] <= value <= bounds[1]:
        raise SpecError(pointer, f"must be {bounds[0]} to {bounds[1]}")
    return value


def checked_age(age: int) -> int:
    if not 0 <= age <= MAX_DOCUMENT_MAX_AGE:
        raise SpecError(
            "/x-mado-limits/documentMaxAge", f"must be 0 to {MAX_DOCUMENT_MAX_AGE}"
        )
    return age


def parse_buffers(document: Json) -> Buffers:
    node = as_object(document.get("x-mado-limits", {}), "/x-mado-limits")
    known = {"outputBufferSize", "fileChunkSize", "documentMaxAge"}
    unknown = sorted(set(node) - known)
    if unknown:
        raise SpecError(f"/x-mado-limits/{escape(unknown[0])}", "is not a known limit")
    return Buffers(
        buffer_size(node, "outputBufferSize", (MIN_OUTPUT_BUFFER, MAX_OUTPUT_BUFFER)),
        buffer_size(node, "fileChunkSize", (1, MAX_FILE_CHUNK)),
        checked_age(
            as_int(
                node.get("documentMaxAge", DEFAULT_DOCUMENT_MAX_AGE),
                "/x-mado-limits/documentMaxAge",
            )
        ),
    )


def parse_api(document: Json) -> Api:
    version = str(document.get("openapi", ""))
    if not version.startswith("3.1."):
        raise SpecError("/openapi", "must be 3.1.x")
    info = as_object(child(document, "info", ""), "/info")
    components = as_object(document.get("components", {}), "/components")
    parse_schemes(components)
    schema_nodes = as_object(components.get("schemas", {}), "/components/schemas")
    schemas = tuple(
        parse_schema(
            name,
            as_object(node, f"/components/schemas/{escape(name)}"),
            f"/components/schemas/{escape(name)}",
        )
        for name, node in schema_nodes.items()
    )
    secured = parse_security(document.get("security", []), "/security")
    context = (secured, {schema.name for schema in schemas})
    operations = parse_operations(
        as_object(child(document, "paths", ""), "/paths"), context
    )
    return Api(
        str(child(info, "title", "/info")),
        schemas,
        operations,
        parse_buffers(document),
    )
