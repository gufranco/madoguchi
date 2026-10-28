import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.madogen.emit import render_api, same_output
from tools.madogen.spec import Api, Buffers, Field, Kind, Operation, Schema


def item_schema() -> Schema:
    return Schema(
        "Item",
        (
            Field("id", Kind.INTEGER, 0, 1, 254, False, True),
            Field("name", Kind.STRING, 16, 0, 0, True, False),
            Field("value", Kind.INTEGER, 0, -2147483648, 1000, True, False),
            Field("on", Kind.BOOLEAN, 0, 0, 0, False, False),
        ),
    )


def api(operations: tuple[Operation, ...]) -> Api:
    return Api("Items", (item_schema(),), operations)


def operations() -> tuple[Operation, ...]:
    return (
        Operation(
            "getItem",
            "get",
            "/api/items/{id}",
            "/api/items/*",
            "/api/items/",
            False,
            "",
        ),
        Operation(
            "listItems", "get", "/api/items", "/api/items", "", False, "", ("Item",)
        ),
        Operation("createItem", "post", "/api/items", "/api/items", "", True, "Item"),
        Operation(
            "deleteItem",
            "delete",
            "/api/items/{id}",
            "/api/items/*",
            "/api/items/",
            True,
            "",
        ),
    )


def test_lists_route_ids_in_spec_order_then_the_card_routes() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    assert (
        "constexpr uint8_t kGetItem = 0U;\nconstexpr uint8_t kListItems = 1U;\n"
        "constexpr uint8_t kCreateItem = 2U;\nconstexpr uint8_t kDeleteItem = 3U;\n"
        "constexpr uint8_t kOpenApiDocument = 4U;\nconstexpr uint8_t kCardFile = 5U;\n"
        in text
    )


def test_puts_exact_paths_before_wildcards() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    table = text[text.index("const mado::Route kRoutes[]") :]
    order = [
        table.index(entry)
        for entry in (
            "{&kRoutePath0[0], mado::Method::kGet, kOpenApiDocument}",
            "{&kRoutePath1[0], mado::Method::kGet, kListItems}",
            "{&kRoutePath1[0], mado::Method::kPost, kCreateItem}",
            "{&kRoutePath2[0], mado::Method::kGet, kGetItem}",
            "{&kRoutePath2[0], mado::Method::kDelete, kDeleteItem}",
            "{&kRoutePath3[0], mado::Method::kGet, kCardFile}",
            "{nullptr, mado::Method::kAny, 0U}",
        )
    ]
    assert 'const char kRoutePath0[] PROGMEM = "/api/openapi.json";' in text
    assert 'const char kRoutePath1[] PROGMEM = "/api/items";' in text
    assert 'const char kRoutePath2[] PROGMEM = "/api/items/*";' in text
    assert 'const char kRoutePath3[] PROGMEM = "/*";' in text
    assert order == sorted(order)


def test_captures_only_the_headers_the_api_needs() -> None:
    # Arrange
    reads_only = api((operations()[1],))

    # Act
    text = render_api(reads_only, "items")

    # Assert
    assert 'const char kHeaderName0[] PROGMEM = "Accept-Encoding";' in text
    assert 'const char kHeaderName1[] PROGMEM = "If-None-Match";' in text
    assert (
        "const char* const kHeaderNames[] PROGMEM = "
        "{&kHeaderName0[0], &kHeaderName1[0], nullptr};"
    ) in text
    assert "kHeaderPoolSize = 96U;" in text


def test_captures_auth_and_type_headers_for_secured_writes() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    assert 'const char kHeaderName0[] PROGMEM = "Authorization";' in text
    assert 'const char kHeaderName1[] PROGMEM = "Content-Type";' in text
    assert (
        "const char* const kHeaderNames[] PROGMEM = {&kHeaderName0[0], &kHeaderName1[0], "
        "&kHeaderName2[0], &kHeaderName3[0], nullptr};"
    ) in text
    assert "kHeaderPoolSize = 240U;" in text


def test_marks_secured_routes() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    assert (
        "return (!known) || (route == kCreateItem) || (route == kDeleteItem);" in text
    )


def test_leaves_out_token_routing_when_no_route_is_secured() -> None:
    # Arrange
    spec = api(tuple(op for op in operations() if not op.secured))

    # Act
    text = render_api(spec, "items")

    # Assert
    assert "route_needs_token" not in text


def test_asserts_each_field_invariant_on_its_own() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    writer = text[text.index("bool write_item(") :]
    assert writer.index(
        "mado::check((value.name[sizeof(value.name) - 1U] == '\\0'))"
    ) < (writer.index("mado::check((value.id >= 1) && (value.id <= 254))"))


def test_writes_no_assertion_that_cannot_fail() -> None:
    # Arrange
    flag = Schema("Flag", (Field("on", Kind.BOOLEAN, 0, 0, 0, True, False),))
    listing = Operation(
        "listFlags", "get", "/api/flags", "/api/flags", "", False, "", ("Flag",)
    )
    spec = Api("Flags", (flag,), (listing,))

    # Act
    text = render_api(spec, "flags")

    # Assert
    assert "mado::check(true)" not in text
    assert "  const bool sound = true;\n" in text


def test_refuses_a_string_shorter_than_its_min_length() -> None:
    # Arrange
    named = Schema("Tag", (Field("name", Kind.STRING, 16, 1, 0, True, False),))
    create = Operation("createTag", "post", "/api/tags", "/api/tags", "", False, "Tag")
    spec = Api("Tags", (named,), (create,))

    # Act
    text = render_api(spec, "tags")

    # Assert
    assert "strnlen(&value.name[0], sizeof(value.name)) >= 1U" in text
    assert "mado::JsonStatus::kTooShort" in text


def test_skips_the_min_length_check_when_none_is_declared() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    assert "kTooShort" not in text


def test_reads_path_ids_by_route() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    assert (
        "  if ((route == kGetItem) || (route == kDeleteItem)) {\n"
        '    return mado::parse_path_id(path, PSTR("/api/items/"), resource_id);'
        in text
    )


def test_declares_a_struct_and_masks_per_schema() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    assert (
        "struct Item {\n  int32_t id;\n  char name[17];\n  int32_t value;\n  bool on;\n};"
        in text
    )
    assert "constexpr uint8_t kItemName = 0x02U;" in text
    assert "constexpr uint8_t kItemRequired = 0x06U;" in text
    assert "constexpr uint8_t kItemWritable = 0x0EU;" in text


def test_skips_range_checks_that_cannot_fail() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    assert "(number <= 1000)" in text
    assert "(number >= -2147483648)" not in text


def test_serves_the_spec_and_every_other_path_from_the_card() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    assert 'const char kOpenApiCardPath[] PROGMEM = "/openapi.json";' in text
    assert (
        "mado::serve_card_path<kReadMode>(exchange, *card, &kOpenApiCardPath[0]);"
        in text
    )
    assert "return mado::serve_card_file<kReadMode>(exchange, *card);" in text
    assert 'F("the SD card is not available")' in text


def test_embeds_no_asset() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "items")

    # Assert
    assert "const uint8_t kAsset" not in text
    assert re.search(r"<(?:!doctype|html|head|body|script|style|link)\b", text) is None


def test_renders_byte_for_byte_the_same_twice() -> None:
    # Arrange
    spec = api(operations())

    # Act
    first = render_api(spec, "items")

    # Assert
    assert first == render_api(spec, "items")


def test_notices_any_changed_byte() -> None:
    # Arrange
    spec = api(operations())
    expected = render_api(spec, "items")

    # Act
    equal = same_output(expected, expected.replace("kCardFile", "kCardFiles", 1))

    # Assert
    assert not equal
    assert same_output(expected, render_api(spec, "items"))


def test_wraps_everything_in_the_sketch_namespace() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, "rest_get")

    # Assert
    assert "namespace rest_get {\n\nconstexpr uint8_t kGetItem = 0U;" in text
    assert text.endswith("}\n\n}\n\n#endif\n")


def test_emits_a_writer_for_a_request_schema_only_when_marked() -> None:
    # Arrange
    stored = Schema("Item", item_schema().fields, True)
    marked = Api("Items", (stored,), (operations()[2],))
    unmarked = api((operations()[2],))

    # Act
    text = render_api(marked, "items")

    # Assert
    assert "read_item(" in text
    assert "write_item(" in text
    assert "write_item(" not in render_api(unmarked, "items")


def test_emits_a_reader_only_for_request_schemas() -> None:
    # Arrange
    responses_only = api((operations()[1],))

    # Act
    text = render_api(responses_only, "items")

    # Assert
    assert "write_item(" in text
    assert "read_item(" not in text
    assert "struct Item {" in text


def test_emits_a_body_reader_that_names_each_missing_required_field() -> None:
    # Arrange
    writes = api((operations()[2],))

    # Act
    text = render_api(writes, "items")

    # Assert
    assert "mado::Problem read_item_body(ExchangeT& exchange, Item& value" in text
    assert 'F("missing required property: name")' in text
    assert 'F("missing required property: value")' in text
    assert "missing required property: on" not in text
    assert "missing required property: id" not in text


def test_skips_the_body_reader_for_schemas_never_read() -> None:
    # Arrange
    reads = api((operations()[1],))

    # Act
    text = render_api(reads, "items")

    # Assert
    assert "read_item_body(" not in text
    assert "item_missing(" not in text


def test_leaves_out_url_prefixes_when_nothing_is_written() -> None:
    # Arrange
    reads = api(tuple(op for op in operations() if op.method == "get"))

    # Act
    text = render_api(reads, "items")

    # Assert
    assert "get_item_url(" not in text


def test_emits_a_url_prefix_for_each_get_by_id() -> None:
    # Arrange
    everything = api(operations())

    # Act
    text = render_api(everything, "items")

    # Assert
    assert (
        'inline const __FlashStringHelper* get_item_url() {\n  return F("/api/items/");\n}'
        in text
    )
    assert "delete_item_url(" not in text


def test_writes_buffer_limits_and_captures_declared_headers() -> None:
    # Arrange
    upload = Operation(
        "putFile",
        "put",
        "/api/files/{name}",
        "/api/files/*",
        "/api/files/",
        False,
        "",
        (),
        ("Repr-Digest",),
    )
    spec = Api("Files", (item_schema(),), (upload,), Buffers(512, 256))

    # Act
    text = render_api(spec, "files")

    # Assert
    assert "kOutputBufferSize = 512U;" in text
    assert "kFileChunkSize = 256U;" in text
    assert "kMaxCapturedHeaders = 8U;" in text
    assert 'const char kHeaderName2[] PROGMEM = "Repr-Digest";' in text
    assert "kHeaderPoolSize = 240U;" in text


def test_leaves_out_every_card_route_when_the_card_is_off() -> None:
    # Arrange
    spec = Api("Items", (item_schema(),), operations(), Buffers(), False)

    # Act
    text = render_api(spec, "items")

    # Assert
    assert "kOpenApiDocument" not in text
    assert "kCardFile" not in text
    assert "mado::check(route <= kDeleteItem)" in text
    assert "send_card" not in text
    assert "serve_card_file.h" not in text
    assert '"/*"' not in text
    assert "static constexpr uint8_t kMaxRoutes = 4U;" in text
