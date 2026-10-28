# pyright: strict
# SPDX-FileCopyrightText: 2026 Gustavo Franco
# SPDX-License-Identifier: MIT

import re

from .spec import (
    CAPTURED_HEADER_LIMIT,
    INT32_MAX,
    INT32_MIN,
    Api,
    Buffers,
    Field,
    Kind,
    Operation,
    Schema,
)

TOKEN_POOL_SIZE = 240
PLAIN_POOL_SIZE = 96
HEADER = (
    "// SPDX-FileCopyrightText: 2026 Gustavo Franco\n"
    "// SPDX-License-Identifier: MIT\n\n"
    "#ifndef GENERATED_API_H\n#define GENERATED_API_H\n\n"
    "#include <Arduino.h>\n#include <Madoguchi.h>\n#include <stdint.h>\n"
    "#include <string.h>\n#include <mado/BodySource.h>\n"
    "#include <mado/JsonReader.h>\n#include <mado/JsonStatus.h>\n#include <mado/Problem.h>\n"
    "#include <mado/check.h>\n#include <mado/http_status.h>\n#include <mado/json_body_problem.h>\n"
    "#include <mado/json_problem.h>\n#include <mado/json_write_string.h>\n"
    "#include <mado/parse_path_id.h>\n#include <mado/send_problem.h>\n"
)
CARD_INCLUDE = "#include <mado/serve_card_file.h>\n"
FOOTER = "}\n\n#endif\n"
OPENAPI_URL = "/api/openapi.json"
OPENAPI_CARD_PATH = "/openapi.json"


def pascal(name: str) -> str:
    return "".join(part[:1].upper() + part[1:] for part in re.split(r"_", name))


def constant(name: str) -> str:
    return f"k{pascal(name)}"


def snake(name: str) -> str:
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def cpp_string(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def method_enum(method: str) -> str:
    return f"mado::Method::k{method.capitalize()}"


def render_route_ids(operations: tuple[Operation, ...], card: bool) -> str:
    names = [constant(operation.operation_id) for operation in operations]
    extra = ["kOpenApiDocument", "kCardFile"] if card else []
    body = "".join(f"  {name},\n" for name in [*names, *extra])
    return f"enum RouteId : uint8_t {{\n{body}}};\n\n"


def render_routes(operations: tuple[Operation, ...], card: bool) -> str:
    ordered = sorted(
        operations, key=lambda operation: operation.route_pattern.endswith("*")
    )
    edges = ([OPENAPI_URL], ["/*"]) if card else ([], [])
    patterns = list(
        dict.fromkeys(edges[0] + [op.route_pattern for op in ordered] + edges[1])
    )
    paths = "".join(
        f'const char kRoutePath{index}[] PROGMEM = "{pattern}";\n'
        for index, pattern in enumerate(patterns)
    )
    rows = [
        f"{{&kRoutePath{patterns.index(op.route_pattern)}[0], {method_enum(op.method)}, "
        f"{constant(op.operation_id)}}}"
        for op in ordered
    ]
    if card:
        rows = [
            (
                f"{{&kRoutePath{patterns.index(OPENAPI_URL)}[0], mado::Method::kGet, "
                "kOpenApiDocument}"
            ),
            *rows,
            f"{{&kRoutePath{patterns.index('/*')}[0], mado::Method::kGet, kCardFile}}",
        ]
    rows = [*rows, "{nullptr, mado::Method::kAny, 0U}"]
    body = "".join(f"    {row},\n" for row in rows)
    return f"{paths}\nconst mado::Route kRoutes[] PROGMEM = {{\n{body}}};\n\n"


def header_names(operations: tuple[Operation, ...]) -> list[str]:
    names: list[str] = []
    if any(operation.secured for operation in operations):
        names.append("Authorization")
    if any(operation.request_schema for operation in operations):
        names.append("Content-Type")
    extras = [
        name
        for operation in operations
        for name in operation.header_parameters
        if name not in names
    ]
    return list(dict.fromkeys([*names, "Accept-Encoding", "If-None-Match", *extras]))


def render_header_names(operations: tuple[Operation, ...]) -> str:
    names = header_names(operations)
    texts = "".join(
        f'const char kHeaderName{index}[] PROGMEM = "{name}";\n'
        for index, name in enumerate(names)
    )
    pointers = ", ".join(f"&kHeaderName{index}[0]" for index in range(len(names)))
    return f"{texts}const char* const kHeaderNames[] PROGMEM = {{{pointers}, nullptr}};\n\n"


def render_limits(route_count: int, secured: bool, buffers: Buffers) -> str:
    pool = TOKEN_POOL_SIZE if secured else PLAIN_POOL_SIZE
    return (
        "struct Limits {\n"
        "  static constexpr uint16_t kLineBufferSize = 128U;\n"
        f"  static constexpr uint16_t kOutputBufferSize = {buffers.output}U;\n"
        f"  static constexpr uint8_t kHeaderPoolSize = {pool}U;\n"
        "  static constexpr uint8_t kHeaderNameSize = 32U;\n"
        f"  static constexpr uint8_t kMaxCapturedHeaders = {CAPTURED_HEADER_LIMIT}U;\n"
        "  static constexpr uint8_t kMaxHeaderLines = 48U;\n"
        f"  static constexpr uint8_t kMaxRoutes = {route_count}U;\n"
        "  static constexpr uint16_t kReadTimeoutMs = 1000U;\n"
        "  static constexpr uint32_t kMaxPollsPerRead = 100000UL;\n"
        "  static constexpr uint16_t kRequestTimeoutMs = 5000U;\n"
        "  static constexpr uint16_t kMaxDrainBytes = 1024U;\n"
        f"  static constexpr uint16_t kFileChunkSize = {buffers.file_chunk}U;\n"
        "};\n\n"
    )


def case_block(names: list[str], result: str) -> str:
    if not names:
        return ""
    labels = "".join(f"    case {name}:\n" for name in names)
    return f"{labels}      return {result};\n"


def last_route(operations: tuple[Operation, ...], card: bool) -> str:
    return "kCardFile" if card else constant(operations[-1].operation_id)


def render_token_routes(operations: tuple[Operation, ...], last: str) -> str:
    secured = [constant(op.operation_id) for op in operations if op.secured]
    return (
        "inline bool route_needs_token(const uint8_t route) {\n"
        f"  const bool known = mado::check(route <= {last});\n"
        "  if (!known) {\n    return true;\n  }\n"
        "  switch (route) {\n"
        f"{case_block(secured, 'true')}"
        "    default:\n      return false;\n  }\n}\n\n"
    )


def render_path_ids(operations: tuple[Operation, ...], last: str) -> str:
    prefixes = sorted({op.id_prefix for op in operations if op.id_prefix})
    blocks = "".join(
        case_block(
            [constant(op.operation_id) for op in operations if op.id_prefix == prefix],
            f'mado::parse_path_id(path, PSTR("{prefix}"), id)',
        )
        for prefix in prefixes
    )
    return (
        "inline bool route_path_id(const char* const path, const uint8_t route, uint8_t& id) {\n"
        "  id = 0U;\n"
        f"  const bool valid = mado::check(path != nullptr) && mado::check(route <= {last});\n"
        "  if (!valid) {\n    return false;\n  }\n"
        "  switch (route) {\n"
        f"{blocks}"
        "    default:\n      return false;\n  }\n}\n\n"
    )


def render_locations(operations: tuple[Operation, ...]) -> str:
    return "".join(
        f"inline const __FlashStringHelper* {snake(op.operation_id)}_url() {{\n"
        f'  return F("{cpp_string(op.id_prefix)}");\n}}\n\n'
        for op in operations
        if op.id_prefix and op.method == "get"
    )


def field_type(field: Field) -> str:
    if field.kind is Kind.STRING:
        return f"char {field.name}[{field.max_length + 1}]"
    if field.kind is Kind.INTEGER:
        return f"int32_t {field.name}"
    return f"bool {field.name}"


def mask(schema: Schema, chosen: list[Field]) -> str:
    value = sum(1 << schema.fields.index(field) for field in chosen)
    return f"0x{value:02X}U"


def render_struct(schema: Schema) -> str:
    members = "".join(f"  {field_type(field)};\n" for field in schema.fields)
    bits = "".join(
        f"constexpr uint8_t k{schema.name}{pascal(field.name)} = {mask(schema, [field])};\n"
        for field in schema.fields
    )
    writable = [field for field in schema.fields if not field.read_only]
    required = [field for field in writable if field.required]
    key_size = max(len(field.name) for field in schema.fields) + 1
    return (
        f"struct {schema.name} {{\n{members}}};\n\n{bits}"
        f"constexpr uint8_t k{schema.name}Required = {mask(schema, required)};\n"
        f"constexpr uint8_t k{schema.name}Writable = {mask(schema, writable)};\n"
        f"constexpr uint8_t k{schema.name}KeySize = {key_size}U;\n"
        f"constexpr uint8_t k{schema.name}FieldCount = {len(schema.fields)}U;\n\n"
    )


def range_test(field: Field, value: str) -> str:
    tests: list[str] = []
    if field.minimum > INT32_MIN:
        tests.append(f"({value} >= {field.minimum})")
    if field.maximum < INT32_MAX:
        tests.append(f"({value} <= {field.maximum})")
    return " && ".join(tests) if tests else "true"


def field_reader_head(schema: Schema, field: Field) -> str:
    bit = f"k{schema.name}{pascal(field.name)}"
    return (
        "template <typename SourceT>\n"
        f"mado::JsonStatus read_{snake(schema.name)}_{field.name}("
        f"mado::JsonReader<SourceT>& reader, {schema.name}& value,\n"
        "                                    uint8_t& present) {\n"
        f"  const bool fresh = mado::check((present & {bit}) == 0U);\n"
        f"  present = static_cast<uint8_t>(present | {bit});\n"
        "  if (!fresh) {\n    return mado::JsonStatus::kUnsupported;\n  }\n"
    )


def field_reader_body(field: Field) -> str:
    if field.kind is Kind.STRING:
        return f"  return reader.read_string(&value.{field.name}[0], sizeof(value.{field.name}));\n"
    if field.kind is Kind.BOOLEAN:
        return f"  return reader.read_boolean(value.{field.name});\n"
    return (
        "  int32_t number = 0;\n"
        "  const mado::JsonStatus status = reader.read_integer(number);\n"
        f"  const bool in_range = mado::check({range_test(field, 'number')});\n"
        f"  value.{field.name} = in_range ? number : value.{field.name};\n"
        "  const bool rejected = (status == mado::JsonStatus::kOk) && (!in_range);\n"
        "  return rejected ? mado::JsonStatus::kOutOfRange : status;\n"
    )


def render_field_reader(schema: Schema, field: Field) -> str:
    return f"{field_reader_head(schema, field)}{field_reader_body(field)}}}\n\n"


def render_member_reader(schema: Schema) -> str:
    prefix = snake(schema.name)
    branches = "".join(
        f'  if (strcmp_P(key, PSTR("{field.name}")) == 0) {{\n'
        f"    return read_{prefix}_{field.name}(reader, value, present);\n  }}\n"
        for field in schema.fields
        if not field.read_only
    )
    return (
        "template <typename SourceT>\n"
        f"mado::JsonStatus read_{prefix}_member(mado::JsonReader<SourceT>& reader, "
        f"const char* const key,\n"
        f"                                    {schema.name}& value, uint8_t& present) {{\n"
        "  const bool valid = mado::check(key != nullptr) && "
        f"mado::check((present & static_cast<uint8_t>(~k{schema.name}Writable)) == 0U);\n"
        "  if (!valid) {\n    return mado::JsonStatus::kSyntax;\n  }\n"
        f"{branches}"
        "  return mado::JsonStatus::kUnsupported;\n}\n\n"
    )


def render_object_reader(schema: Schema) -> str:
    prefix = snake(schema.name)
    return (
        "template <typename SourceT>\n"
        f"mado::JsonStatus read_{prefix}(mado::JsonReader<SourceT>& reader, {schema.name}& value,\n"
        "                                uint8_t& present) {\n"
        "  present = 0U;\n"
        "  mado::JsonStatus status = reader.begin_object();\n"
        f"  for (uint8_t i = 0U; (status == mado::JsonStatus::kOk) && (i <= k{schema.name}FieldCount); ++i) {{\n"
        f"    char key[k{schema.name}KeySize] = {{}};\n"
        "    status = reader.next_key(&key[0], sizeof(key));\n"
        "    if (status == mado::JsonStatus::kEnd) {\n      return reader.finish();\n    }\n"
        "    if (status == mado::JsonStatus::kOk) {\n"
        f"      status = read_{prefix}_member(reader, &key[0], value, present);\n    }}\n"
        "  }\n"
        "  const bool failed = mado::check(status != mado::JsonStatus::kEnd);\n"
        "  return (failed && (status != mado::JsonStatus::kOk)) ? status : "
        "mado::JsonStatus::kUnsupported;\n}\n\n"
    )


def writer_step(field: Field, first: bool) -> str:
    separator = "{" if first else ","
    label = f'  written = written && (exchange.print(F("{cpp_string(separator)}\\"{field.name}\\":")) > 0U);\n'
    if field.kind is Kind.STRING:
        value = f"mado::json_write_string(exchange, &value.{field.name}[0])"
    elif field.kind is Kind.INTEGER:
        value = f"(exchange.print_signed(value.{field.name}) > 0U)"
    else:
        value = f'(exchange.print(value.{field.name} ? F("true") : F("false")) > 0U)'
    return f"{label}  written = written && {value};\n"


def writer_checks(schema: Schema) -> str:
    tests = [
        f"(value.{field.name}[sizeof(value.{field.name}) - 1U] == '\\0')"
        for field in schema.fields
        if field.kind is Kind.STRING
    ]
    tests += [
        range_test(field, f"value.{field.name}")
        for field in schema.fields
        if field.kind is Kind.INTEGER and range_test(field, "v") != "true"
    ]
    joined = " && ".join(tests) if tests else "true"
    return f"  const bool sound = mado::check({joined});\n"


def render_writer(schema: Schema) -> str:
    steps = "".join(
        writer_step(field, index == 0) for index, field in enumerate(schema.fields)
    )
    return (
        "template <typename ExchangeT>\n"
        f"bool write_{snake(schema.name)}(ExchangeT& exchange, const {schema.name}& value) {{\n"
        f"{writer_checks(schema)}"
        "  bool written = sound;\n"
        f"{steps}"
        '  return written && (exchange.print(F("}")) > 0U);\n}\n\n'
    )


def render_missing(schema: Schema) -> str:
    prefix = snake(schema.name)
    required = [
        field for field in schema.fields if field.required and not field.read_only
    ]
    branches = "".join(
        f"  if ((present & k{schema.name}{pascal(field.name)}) == 0U) {{\n"
        "    return mado::Problem{mado::http::kUnprocessableContent, "
        f'F("missing required property: {field.name}")}};\n  }}\n'
        for field in required
    )
    return (
        f"inline mado::Problem {prefix}_missing(const uint8_t present) {{\n"
        "  const bool sound = mado::check("
        f"(present & static_cast<uint8_t>(~k{schema.name}Writable)) == 0U);\n"
        "  if (!sound) {\n"
        "    return mado::Problem{mado::http::kUnprocessableContent, nullptr};\n  }\n"
        f"{branches}"
        "  return mado::Problem{mado::http::kNoResponse, nullptr};\n}\n\n"
    )


def render_body_reader(schema: Schema) -> str:
    prefix = snake(schema.name)
    return (
        "template <typename ExchangeT>\n"
        f"mado::Problem read_{prefix}_body(ExchangeT& exchange, {schema.name}& value, "
        "uint8_t& present,\n"
        "                                const uint32_t max_body) {\n"
        "  present = 0U;\n"
        "  const mado::Problem framing = mado::json_body_problem(exchange, max_body);\n"
        "  if (framing.status != mado::http::kNoResponse) {\n    return framing;\n  }\n"
        "  mado::BodySource<ExchangeT> source(exchange);\n"
        "  mado::JsonReader<mado::BodySource<ExchangeT>> reader(source);\n"
        f"  const mado::JsonStatus status = read_{prefix}(reader, value, present);\n"
        "  if (status != mado::JsonStatus::kOk) {\n"
        "    return mado::json_problem(status);\n  }\n"
        f"  return {prefix}_missing(present);\n}}\n\n"
    )


def render_readers(schema: Schema) -> str:
    fields = "".join(
        render_field_reader(schema, field)
        for field in schema.fields
        if not field.read_only
    )
    return (
        fields
        + render_member_reader(schema)
        + render_object_reader(schema)
        + render_missing(schema)
        + render_body_reader(schema)
    )


def render_schema(schema: Schema, operations: tuple[Operation, ...]) -> str:
    read = any(operation.request_schema == schema.name for operation in operations)
    written = schema.stored or any(
        schema.name in operation.response_schemas for operation in operations
    )
    return (
        render_struct(schema)
        + (render_readers(schema) if read else "")
        + (render_writer(schema) if written else "")
    )


def render_send_card() -> str:
    return (
        f'const char kOpenApiCardPath[] PROGMEM = "{OPENAPI_CARD_PATH}";\n\n'
        "template <uint16_t kReadMode, typename ExchangeT, typename SdT>\n"
        "bool send_card(ExchangeT& exchange, SdT* const card, const uint8_t route) {\n"
        "  const bool served = mado::check(route == kOpenApiDocument) ||\n"
        "                      mado::check(route == kCardFile);\n"
        "  if (!served) {\n    return exchange.send_empty(mado::http::kNotFound);\n  }\n"
        "  if (card == nullptr) {\n"
        "    return mado::send_problem(\n"
        "        exchange, mado::Problem{mado::http::kServiceUnavailable,\n"
        '                                F("the SD card is not available")});\n  }\n'
        "  if (route == kOpenApiDocument) {\n"
        "    return mado::serve_card_path<kReadMode>(exchange, *card, &kOpenApiCardPath[0]);\n"
        "  }\n"
        "  return mado::serve_card_file<kReadMode>(exchange, *card);\n}\n\n"
    )


def render_api(api: Api, namespace: str) -> str:
    route_count = len(api.operations) + (2 if api.card else 0)
    return (
        HEADER
        + (CARD_INCLUDE if api.card else "")
        + f"\nnamespace {namespace} {{\n\n"
        + render_route_ids(api.operations, api.card)
        + render_routes(api.operations, api.card)
        + render_header_names(api.operations)
        + render_limits(
            route_count,
            any(op.secured or op.header_parameters for op in api.operations),
            api.buffers,
        )
        + render_token_routes(api.operations, last_route(api.operations, api.card))
        + render_path_ids(api.operations, last_route(api.operations, api.card))
        + render_locations(api.operations)
        + "".join(render_schema(schema, api.operations) for schema in api.schemas)
        + (render_send_card() if api.card else "")
        + FOOTER
    )


def same_output(expected: str, actual: str) -> bool:
    return expected == actual
