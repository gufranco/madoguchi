import gzip
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.madogen.assets import Asset, make_asset
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


def page() -> Asset:
    return make_asset(
        ("/", "/index.html"),
        "text/html; charset=utf-8",
        b"<p>hi</p>",
        "default-src 'none'",
    )


def test_lists_route_ids_in_spec_order_then_the_static_route() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, (page(),), "items")

    # Assert
    assert (
        "enum RouteId : uint8_t {\n  kGetItem,\n  kListItems,\n  kCreateItem,\n  kDeleteItem,\n  kStaticAsset,\n};"
        in text
    )


def test_puts_exact_paths_before_wildcards() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, (page(),), "items")

    # Assert
    table = text[text.index("const mado::Route kRoutes[]") :]
    order = [
        table.index(entry)
        for entry in (
            "{&kRoutePath0[0], mado::Method::kGet, kListItems}",
            "{&kRoutePath0[0], mado::Method::kPost, kCreateItem}",
            "{&kRoutePath1[0], mado::Method::kGet, kGetItem}",
            "{&kRoutePath1[0], mado::Method::kDelete, kDeleteItem}",
            "{&kRoutePath2[0], mado::Method::kGet, kStaticAsset}",
            "{nullptr, mado::Method::kAny, 0U}",
        )
    ]
    assert 'const char kRoutePath0[] PROGMEM = "/api/items";' in text
    assert 'const char kRoutePath1[] PROGMEM = "/api/items/*";' in text
    assert 'const char kRoutePath2[] PROGMEM = "/*";' in text
    assert order == sorted(order)


def test_captures_only_the_headers_the_api_needs() -> None:
    # Arrange
    reads_only = api((operations()[1],))

    # Act
    text = render_api(reads_only, (page(),), "items")

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
    text = render_api(spec, (page(),), "items")

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
    text = render_api(spec, (page(),), "items")

    # Assert
    assert "    case kCreateItem:\n    case kDeleteItem:\n      return true;" in text


def test_reads_path_ids_by_route() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, (page(),), "items")

    # Assert
    assert (
        '    case kGetItem:\n    case kDeleteItem:\n      return mado::parse_path_id(path, PSTR("/api/items/"), id);'
        in text
    )


def test_declares_a_struct_and_masks_per_schema() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, (page(),), "items")

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
    text = render_api(spec, (page(),), "items")

    # Assert
    assert "(number <= 1000)" in text
    assert "(number >= -2147483648)" not in text


def test_embeds_assets_and_their_paths() -> None:
    # Arrange
    spec = api(operations())
    asset = page()

    # Act
    text = render_api(spec, (asset,), "items")

    # Assert
    assert f"const uint8_t kAsset0[{len(asset.data)}] PROGMEM = {{" in text
    assert (
        'if (strcmp_P(path, PSTR("/index.html")) == 0) {\n    return 0U;\n  }' in text
    )
    assert asset.etag.replace('"', '\\"') in text


def test_renders_byte_for_byte_the_same_twice() -> None:
    # Arrange
    spec = api(operations())

    # Act
    first = render_api(spec, (page(),), "items")

    # Assert
    assert first == render_api(spec, (page(),), "items")


def test_compares_assets_by_their_content() -> None:
    # Arrange
    spec = api(operations())
    expected = render_api(spec, (page(),), "items")
    source = b"<p>hi</p>"
    stored = gzip.compress(source, compresslevel=1, mtime=0)
    other = Asset(
        page().url_paths,
        page().content_type,
        stored,
        page().etag,
        page().policy,
        page().cache_control,
    )
    actual = render_api(spec, (other,), "items")

    # Act
    equal = same_output(expected, actual)

    # Assert
    assert expected != actual
    assert equal


def test_notices_a_changed_asset() -> None:
    # Arrange
    spec = api(operations())
    expected = render_api(spec, (page(),), "items")
    changed = make_asset(
        page().url_paths, page().content_type, b"<p>bye</p>", page().policy
    )
    actual = render_api(spec, (changed,), "items")

    # Act
    equal = same_output(expected, actual)

    # Assert
    assert not equal


def test_wraps_everything_in_the_sketch_namespace() -> None:
    # Arrange
    spec = api(operations())

    # Act
    text = render_api(spec, (page(),), "rest_get")

    # Assert
    assert "namespace rest_get {\n\nenum RouteId" in text
    assert text.endswith("}\n\n}\n\n#endif\n")


def test_emits_a_writer_for_a_request_schema_only_when_marked() -> None:
    # Arrange
    stored = Schema("Item", item_schema().fields, True)
    marked = Api("Items", (stored,), (operations()[2],))
    unmarked = api((operations()[2],))

    # Act
    text = render_api(marked, (page(),), "items")

    # Assert
    assert "read_item(" in text
    assert "write_item(" in text
    assert "write_item(" not in render_api(unmarked, (page(),), "items")


def test_emits_a_reader_only_for_request_schemas() -> None:
    # Arrange
    responses_only = api((operations()[1],))

    # Act
    text = render_api(responses_only, (page(),), "items")

    # Assert
    assert "write_item(" in text
    assert "read_item(" not in text
    assert "struct Item {" in text


def test_emits_a_body_reader_that_names_each_missing_required_field() -> None:
    # Arrange
    writes = api((operations()[2],))

    # Act
    text = render_api(writes, (page(),), "items")

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
    text = render_api(reads, (page(),), "items")

    # Assert
    assert "read_item_body(" not in text
    assert "item_missing(" not in text


def test_emits_a_url_prefix_for_each_get_by_id() -> None:
    # Arrange
    everything = api(operations())

    # Act
    text = render_api(everything, (page(),), "items")

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
    spec = Api("Files", (item_schema(),), (upload,), Buffers(512, 256, 60))

    # Act
    text = render_api(spec, (page(),), "files")

    # Assert
    assert "kOutputBufferSize = 512U;" in text
    assert "kFileChunkSize = 256U;" in text
    assert "kMaxCapturedHeaders = 8U;" in text
    assert 'const char kHeaderName2[] PROGMEM = "Repr-Digest";' in text
    assert "kHeaderPoolSize = 240U;" in text
