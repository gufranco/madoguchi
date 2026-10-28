import copy
import sys
from pathlib import Path
from typing import Any

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.madogen.spec import Kind, SpecError, parse_api


def document() -> dict[str, Any]:
    item = {
        "type": "object",
        "additionalProperties": False,
        "required": ["name", "value"],
        "properties": {
            "id": {"type": "integer", "minimum": 1, "maximum": 254, "readOnly": True},
            "name": {"type": "string", "maxLength": 16},
            "value": {"type": "integer", "minimum": -1000, "maximum": 1000},
            "on": {"type": "boolean"},
        },
    }
    body = {
        "required": True,
        "content": {
            "application/json": {"schema": {"$ref": "#/components/schemas/Item"}}
        },
    }
    return {
        "openapi": "3.1.0",
        "info": {"title": "Items", "version": "1.0.0"},
        "components": {
            "schemas": {"Item": item},
            "securitySchemes": {
                "bearerAuth": {
                    "type": "http",
                    "scheme": "bearer",
                    "bearerFormat": "JWT",
                }
            },
        },
        "paths": {
            "/api/items": {
                "get": {"operationId": "listItems", "responses": {}},
                "post": {
                    "operationId": "createItem",
                    "security": [{"bearerAuth": []}],
                    "requestBody": body,
                    "responses": {},
                },
            },
            "/api/items/{id}": {
                "delete": {
                    "operationId": "deleteItem",
                    "security": [{"bearerAuth": []}],
                    "responses": {},
                }
            },
        },
    }


def error_for(spec: dict[str, Any]) -> SpecError:
    with pytest.raises(SpecError) as caught:
        parse_api(spec)
    return caught.value


def test_reads_operations_in_document_order() -> None:
    # Arrange
    spec = document()

    # Act
    api = parse_api(spec)

    # Assert
    assert [operation.operation_id for operation in api.operations] == [
        "listItems",
        "createItem",
        "deleteItem",
    ]
    assert [operation.secured for operation in api.operations] == [False, True, True]
    assert api.operations[1].request_schema == "Item"
    assert api.operations[2].route_pattern == "/api/items/*"
    assert api.operations[2].id_prefix == "/api/items/"


def test_reads_schema_fields_with_their_limits() -> None:
    # Arrange
    spec = document()

    # Act
    api = parse_api(spec)

    # Assert
    fields = api.schemas[0].fields
    assert [field.name for field in fields] == ["id", "name", "value", "on"]
    assert [field.kind for field in fields] == [
        Kind.INTEGER,
        Kind.STRING,
        Kind.INTEGER,
        Kind.BOOLEAN,
    ]
    assert fields[0].read_only
    assert fields[1].max_length == 16
    assert (fields[2].minimum, fields[2].maximum) == (-1000, 1000)
    assert [field.required for field in fields] == [False, True, True, False]


def test_applies_document_level_security() -> None:
    # Arrange
    spec = document()
    spec["security"] = [{"bearerAuth": []}]
    spec["paths"]["/api/items"]["get"]["security"] = []

    # Act
    api = parse_api(spec)

    # Assert
    assert [operation.secured for operation in api.operations] == [False, True, True]


def mutate(path: list[str], value: Any) -> dict[str, Any]:
    spec = copy.deepcopy(document())
    node: Any = spec
    for key in path[:-1]:
        node = node[key]
    node[path[-1]] = value
    return spec


@pytest.mark.parametrize(
    ("spec", "pointer"),
    [
        (mutate(["openapi"], "3.0.3"), "/openapi"),
        (
            mutate(["paths", "/api/items", "post", "operationId"], "listItems"),
            "/paths/~1api~1items/post/operationId",
        ),
        (
            mutate(["paths", "/api/items", "get", "operationId"], "List-Items"),
            "/paths/~1api~1items/get/operationId",
        ),
        (mutate(["paths", "/items"], {}), "/paths/~1items"),
        (mutate(["paths", "/api/{id}/items"], {}), "/paths/~1api~1{id}~1items"),
        (
            mutate(["paths", "/api/items", "post", "security"], [{"basic": []}]),
            "/paths/~1api~1items/post/security",
        ),
        (
            mutate(
                ["components", "schemas", "Item", "properties", "name"],
                {"type": "string"},
            ),
            "/components/schemas/Item/properties/name",
        ),
        (
            mutate(
                ["components", "schemas", "Item", "properties", "tags"],
                {"type": "array"},
            ),
            "/components/schemas/Item/properties/tags/type",
        ),
        (
            mutate(["components", "schemas", "Item", "additionalProperties"], True),
            "/components/schemas/Item",
        ),
        (
            mutate(["components", "schemas", "Item", "required"], ["missing"]),
            "/components/schemas/Item/required",
        ),
        (
            mutate(
                ["paths", "/api/items", "post", "requestBody", "content"],
                {"text/plain": {}},
            ),
            "/paths/~1api~1items/post/requestBody/content",
        ),
        (
            mutate(["components", "securitySchemes", "bearerAuth", "scheme"], "basic"),
            "/components/securitySchemes/bearerAuth",
        ),
    ],
)
def test_names_the_offending_node(spec: dict[str, Any], pointer: str) -> None:
    # Arrange
    expected = pointer

    # Act
    error = error_for(spec)

    # Assert
    assert error.pointer == expected


def test_rejects_more_operations_than_the_route_table_holds() -> None:
    # Arrange
    spec = document()
    for index in range(31):
        spec["paths"][f"/api/extra{index}"] = {
            "get": {"operationId": f"extra{index}", "responses": {}}
        }

    # Act
    error = error_for(spec)

    # Assert
    assert error.pointer == "/paths"


def test_collects_response_schemas_from_objects_and_arrays() -> None:
    # Arrange
    spec = document()
    item_ref = {"$ref": "#/components/schemas/Item"}
    spec["paths"]["/api/items"]["get"]["responses"] = {
        "200": {
            "description": "all",
            "content": {
                "application/json": {"schema": {"type": "array", "items": item_ref}}
            },
        }
    }
    spec["paths"]["/api/items"]["post"]["responses"] = {
        "201": {
            "description": "one",
            "content": {"application/json": {"schema": item_ref}},
        },
        "400": {"description": "bad"},
    }

    # Act
    api = parse_api(spec)

    # Assert
    assert [operation.response_schemas for operation in api.operations] == [
        ("Item",),
        ("Item",),
        (),
    ]


def header_parameter(name: str) -> dict[str, Any]:
    return {"name": name, "in": "header", "schema": {"type": "string"}}


def test_accepts_octet_stream_bodies_without_a_schema() -> None:
    # Arrange
    spec = document()
    spec["paths"]["/api/items"]["post"]["requestBody"] = {
        "required": True,
        "content": {"application/octet-stream": {}},
    }

    # Act
    api = parse_api(spec)

    # Assert
    assert api.operations[1].request_schema == ""


def test_collects_header_parameters_and_skips_other_locations() -> None:
    # Arrange
    spec = document()
    spec["paths"]["/api/items"]["post"]["parameters"] = [
        header_parameter("Repr-Digest"),
        {"name": "page", "in": "query", "schema": {"type": "integer"}},
    ]

    # Act
    api = parse_api(spec)

    # Assert
    assert api.operations[1].header_parameters == ("Repr-Digest",)
    assert api.operations[0].header_parameters == ()


def test_reads_buffer_limits_and_defaults_the_rest() -> None:
    # Arrange
    spec = document()
    spec["x-mado-limits"] = {"outputBufferSize": 512}

    # Act
    api = parse_api(spec)

    # Assert
    assert (api.buffers.output, api.buffers.file_chunk) == (512, 32)
    assert parse_api(document()).buffers.output == 32


@pytest.mark.parametrize(
    ("limits", "pointer"),
    [
        ({"outputBufferSize": 7}, "/x-mado-limits/outputBufferSize"),
        ({"outputBufferSize": 1461}, "/x-mado-limits/outputBufferSize"),
        ({"fileChunkSize": 0}, "/x-mado-limits/fileChunkSize"),
        ({"fileChunkSize": 1025}, "/x-mado-limits/fileChunkSize"),
        ({"documentMaxAge": 60}, "/x-mado-limits/documentMaxAge"),
        ({"keepAlive": 1}, "/x-mado-limits/keepAlive"),
    ],
)
def test_names_the_limit_out_of_range(limits: dict[str, int], pointer: str) -> None:
    # Arrange
    spec = document()
    spec["x-mado-limits"] = limits

    # Act
    error = error_for(spec)

    # Assert
    assert error.pointer == pointer


def test_rejects_a_header_parameter_that_is_not_a_token() -> None:
    # Arrange
    spec = document()
    spec["paths"]["/api/items"]["post"]["parameters"] = [header_parameter("Bad Name")]

    # Act
    error = error_for(spec)

    # Assert
    assert error.pointer == "/paths/~1api~1items/post/parameters/0/name"


def test_rejects_more_header_parameters_than_the_server_captures() -> None:
    # Arrange
    spec = document()
    names = ["X-One", "X-Two", "X-Three", "X-Four", "X-Five", "Content-Type"]
    spec["paths"]["/api/items"]["post"]["parameters"] = [
        header_parameter(name) for name in names
    ]
    allowed = document()
    allowed["paths"]["/api/items"]["post"]["parameters"] = [
        header_parameter(name) for name in names[1:]
    ]

    # Act
    error = error_for(spec)

    # Assert
    assert error.pointer == "/paths"
    assert "at most 4 header parameters" in str(error)
    assert len(parse_api(allowed).operations[1].header_parameters) == 5


def test_marks_a_schema_the_sketch_writes() -> None:
    # Arrange
    spec = document()
    spec["components"]["schemas"]["Item"]["x-mado-writer"] = True

    # Act
    api = parse_api(spec)

    # Assert
    assert api.schemas[0].stored
    assert not parse_api(document()).schemas[0].stored


def test_rejects_a_writer_mark_that_is_not_a_boolean() -> None:
    # Arrange
    spec = document()
    spec["components"]["schemas"]["Item"]["x-mado-writer"] = "yes"

    # Act
    error = error_for(spec)

    # Assert
    assert error.pointer == "/components/schemas/Item/x-mado-writer"


def test_serves_the_card_unless_the_document_turns_it_off() -> None:
    # Arrange
    spec = document()
    spec["x-mado-card"] = False

    # Act
    api = parse_api(spec)

    # Assert
    assert not api.card
    assert parse_api(document()).card


def test_rejects_a_card_switch_that_is_not_a_boolean() -> None:
    # Arrange
    spec = document()
    spec["x-mado-card"] = "no"

    # Act
    error = error_for(spec)

    # Assert
    assert error.pointer == "/x-mado-card"
