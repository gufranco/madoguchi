<div align="center">

<h1>Madoguchi</h1>

<strong>Heap-free HTTP server for the Arduino Uno R4 Minima that serves its pages from the SD card and a JWT-secured REST API generated from OpenAPI.</strong>

<br>
<br>

English | [日本語](README.ja.md) | [简体中文](README.zh-Hans.md) | [繁體中文 (香港)](README.zh-HK.md)

[![CI](https://github.com/gufranco/madoguchi/actions/workflows/ci.yml/badge.svg)](https://github.com/gufranco/madoguchi/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

</div>

<p align="center">
  <a href="#quick-start">Quick start</a> |
  <a href="#examples">Examples</a> |
  <a href="#the-sd-card-web-projects">SD card pages</a> |
  <a href="#rest-api-with-openapi-and-jwt">REST API</a> |
  <a href="#development">Development</a> |
  <a href="CHANGELOG.md">Changelog</a>
</p>

---

Madoguchi runs on the Uno R4 Minima with a W5x00 or ENC28J60 shield and an SD card, and allocates nothing on the heap. The C++ code holds no HTML, CSS, script or image: every page lives on the card and comes from a separate web project per example, written in TypeScript with classless Pico CSS, built by Vite into the card's files with gzip copies. The board answers the REST API generated from one OpenAPI document, with JWT login, JSON bodies and every HTTP verb. CI builds all 9 examples, fails any build over the board's flash or RAM budget, runs 1,182 host tests, 91 tool tests, 106 page tests and 32 browser tests, builds every card, and fuzzes the parsers on every push and pull request.

The name is 窓口, Japanese for the service counter where you hand over a request and get an answer back.

## Usage

A handler is a static function that switches on a route id. Pages come from the card, and the API comes from code. The core of a sketch:

```cpp
struct WebApp {
  static bool handle(WebExchange& exchange, const uint8_t route) {
    switch (route) {
      case kStatus: return send_status_json(exchange);
      default: return mado::serve_card_file<FILE_READ>(exchange, sd_card);
    }
  }
};

const char kStatusPath[] PROGMEM = "/api/status";
const char kCardPath[] PROGMEM = "/*";
const mado::Route kRoutes[] PROGMEM = {
    {&kStatusPath[0], mado::Method::kGet, kStatus},
    {&kCardPath[0], mado::Method::kGet, kCard},
    {nullptr, mado::Method::kAny, 0U},
};
mado::Server<EthernetClient, WebApp> web(&kRoutes[0], &kHeaderNames[0]);

void loop() {
  EthernetClient client = ethernet_server.available();
  const bool has_client = static_cast<bool>(client);
  if (has_client) {
    web.process(client);
  }
}
```

`process()` returns at once when no request is waiting. Otherwise it reads the request, calls `WebApp::handle` with the id of the matching route, and closes the connection. `serve_card_file()` maps `/` to `/index.html`, prefers the `.gz` copy next to a file when the browser accepts gzip, and caches hashed file names for a year.

## Quick start

### Prerequisites

| Tool | Version | Needed for |
|:-----|:--------|:-----------|
| [arduino-cli](https://arduino.github.io/arduino-cli/) or the Arduino IDE | arduino-cli 1.5.1 is what CI uses | Building for the board |
| `arduino:renesas_uno` core | 1.6.0 | The Uno R4 Minima |
| Ethernet and SdFat libraries | Ethernet 2.0.2, SdFat 2.3.0 | Every example |
| Node.js and pnpm | Node 24, pnpm 12.15.0 | Building the pages for the card |
| CMake and a C++ compiler | CMake 3.24 or later | Running the REST API on a computer |

### Install

Madoguchi is not in the Arduino Library Manager. Install it from this repository:

```sh
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --git-url https://github.com/gufranco/madoguchi.git
arduino-cli lib install Ethernet@2.0.2 SdFat@2.3.0
```

In the Arduino IDE, download the repository as a ZIP and use **Sketch > Include Library > Add .ZIP Library**.

### Build the board and the card

From a clone of this repository, with the board on USB:

```sh
git clone https://github.com/gufranco/madoguchi.git
cd madoguchi
arduino-cli compile --fqbn arduino:renesas_uno:minima --library . examples/RestGet
arduino-cli board list
arduino-cli upload --fqbn arduino:renesas_uno:minima --port <port from board list> examples/RestGet
corepack enable
pnpm install
pnpm --filter ./examples/RestGet/SD\ Card run build
```

Copy everything in `examples/RestGet/SD Card/dist` to the root of a FAT16, FAT32 or exFAT microSD card and put it in the shield. [`RestFormat`](examples/RestFormat/RestFormat.ino) prepares a card. The sketch uses the address 192.168.1.177; change it if your network uses another range. Then open `http://192.168.1.177/`.

### Try the REST API without a board

From the same clone, `mado_host_server` runs the [`RestApi`](examples/RestApi/RestApi.ino) application code on 127.0.0.1, with a test signing key and the password `correct-horse-battery`. Its third argument is a folder that stands in for the card:

```sh
pnpm --filter ./examples/RestApi/SD\ Card run build
cmake -S tests -B build
cmake --build build --target mado_host_server
build/mado_host_server 8080 100000 "examples/RestApi/SD Card/dist"
```

In a second terminal:

```sh
TOKEN=$(curl -s -H 'Content-Type: application/json' \
  -d '{"username":"admin","password":"correct-horse-battery"}' \
  http://127.0.0.1:8080/api/auth/login | python3 -c 'import json,sys; print(json.load(sys.stdin)["access_token"])')
curl -s -i -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d '{"name":"lamp","value":3,"on":true}' http://127.0.0.1:8080/api/items
```

Expected output:

```text
HTTP/1.1 201 Created
Connection: close
Content-Type: application/json
Cache-Control: no-store
Location: /api/items/1

{"id":1,"name":"lamp","value":3,"on":true}
```

Open `http://127.0.0.1:8080/` for the site and `http://127.0.0.1:8080/docs/` for Swagger UI. During page work, `pnpm --filter ./examples/RestApi/SD\ Card run dev` serves the pages with live reload and sends `/api` to the board named in `MADOGUCHI_BOARD`, `http://madoguchi.local` by default.

## Supported hardware

Madoguchi supports one board: the Arduino Uno R4 Minima, a Renesas RA4M1. No other board is a target, the Uno R3 and the Uno R4 WiFi included.

| Hardware | Library | Client type |
|:---------|:--------|:------------|
| Wiznet W5100, W5200, W5500 shields | Ethernet 2.0.2 | `EthernetClient` |
| ENC28J60 modules | EthernetENC 2.0.5 | `EthernetClient` |
| microSD slot on the shield | SdFat 2.3.0 | `SdFs` and `FsFile` |

The server is a template on the client type, so any class with `available()`, `read()`, `read(buffer, size)`, `write(buffer, size)`, `connected()` and `stop()` works. SdFat reads long file names on FAT16, FAT32 and exFAT. The Arduino SD library reads only 8.3 names, so Madoguchi uses SdFat. Include `<SdFat.h>` after every Madoguchi header: the R4 core defines `F()` as a function, and SdFat replaces it with a macro that drops the flash type.

## Examples

Sizes in bytes on the Uno R4 Minima, built with arduino-cli 1.5.1, arduino:renesas_uno 1.6.0, Ethernet 2.0.2 and SdFat 2.3.0. Each example but RestFormat has an `SD Card` folder with its pages:

| Example | Shows | Flash, of 262,144 | RAM used, of 32,768 |
|:--------|:------|------:|------:|
| [`RestGet`](examples/RestGet/RestGet.ino) | `GET` of the item list and of one item | 74,072 | 5,920 |
| [`RestPost`](examples/RestPost/RestPost.ino) | `POST` of a new item as JSON | 76,340 | 5,920 |
| [`RestPut`](examples/RestPut/RestPut.ino) | `PUT` replacing an item | 76,540 | 5,920 |
| [`RestPatch`](examples/RestPatch/RestPatch.ino) | `PATCH` changing some fields of an item | 76,508 | 5,920 |
| [`RestDelete`](examples/RestDelete/RestDelete.ino) | `DELETE` of an item by id | 73,316 | 5,920 |
| [`RestAuth`](examples/RestAuth/RestAuth.ino) | login and one protected route | 86,920 | 6,144 |
| [`RestFiles`](examples/RestFiles/RestFiles.ino) | files on SD: list, download, ranges, upload with progress, delete | 94,040 | 6,936 |
| [`RestApi`](examples/RestApi/RestApi.ino) | the full API, its site, Swagger UI and the request log | 105,644 | 6,632 |
| [`RestFormat`](examples/RestFormat/RestFormat.ino) | wipe with 0x00 and 0xFF, verify, then format as FAT16, FAT32 or exFAT; JSON only | 71,440 | 9,936 |

Every example but RestFormat answers `/api/openapi.json` from the card and serves Swagger UI at `/docs/`. CI fails a build that crosses the board's budget.

## The SD card web projects

Every file a browser loads comes from the card. Each example's `SD Card` folder is a web project of its own: semantic HTML, TypeScript in strict mode, classless [Pico CSS](https://picocss.com/) with light and dark mode, and Vitest tests with coverage. The workspace builds them all:

```sh
corepack enable
pnpm install
pnpm run check
pnpm run test
pnpm run build
```

| Step | Tool | What it does |
|:-----|:-----|:-------------|
| Check | Biome 2.6.3, TypeScript 7.0.2, html-validate 11.17.0 | lint and format, strict type check, HTML standards |
| Test | Vitest 5.0.5 with happy-dom | each page against a fake board, 95 percent coverage gate |
| Build | Vite 8.3.5 and `@madoguchi/kit/vite` | TypeScript bundled into files named after an 8-character hex hash, HTML minified, a `.gz` copy next to every file that shrinks, the OpenAPI document copied to `openapi.json`, every name checked against what the board serves |
| Browser | Playwright 1.64.0 with Chromium | each built page and its docs page at 320 px and 1280 px, the board faked by route interception: headings, computed styles, no horizontal scroll, the theme toggle, a keyboard focus ring, every Swagger operation, an empty console |

Third-party front-end code never goes on the card. Pico CSS 2.1.1 loads from jsDelivr and Swagger UI 5.33.2 from unpkg, each pinned to an exact version with a Subresource Integrity hash. The build writes a Content-Security-Policy into each page that allows only the page's own files and those CDN hosts. The theme follows the system setting until the visitor presses the toggle, and the choice is kept in the browser.

The shared code lives in [`web/kit`](web/kit): the CDN pins, the theme toggle, the fetch helpers that read RFC 9457 problems, the bearer session kept in memory only, and the Vite configuration.

## Architecture

```mermaid
graph LR
    subgraph Build time on a computer
        Spec[openapi.json] --> Gen[tools/madogen]
        Gen --> Header[generated_api.h]
        Pages[SD Card web project] --> Vite[Vite and the kit]
        Spec --> Vite
        Vite --> Card[Card files and gz copies]
    end
    subgraph Board
        Client[EthernetClient] --> Server[mado::Server process]
        Server --> Lookup[Route table lookup]
        Lookup --> App[AppT handle by route id]
        App --> Exchange[mado::Exchange output buffer]
        App --> SD[serve_card_file from SD]
        SD --> Exchange
        Exchange --> Client
    end
    Header --> Lookup
    Card --> SD
```

- No asset lives in C++ code. Code writes only data: JSON, RFC 9457 problems and short status messages.
- Buffers are fixed and sized by a `Limits` type passed as a template argument. Nothing is allocated on the heap.
- The library holds no function pointers and declares no virtual functions. A static `handle` that switches on the route id keeps the call graph visible to analyzers.
- Every loop has an iteration cap, every read has a timeout and a whole request has a deadline, so a slow client cannot hang the sketch.
- Requests are parsed as RFC 9112 requires: an `HTTP/1.1` request needs exactly one `Host`, a repeated `Content-Length` gets `400`, `Transfer-Encoding` gets `501`, and a body the handler left unread is read and dropped before the connection closes.

The code follows the JPL D-60411 C standard and the Power of Ten rules at their strictest reading, adapted to C++. [`AGENTS.md`](AGENTS.md) lists every rule and how it applies.

## What's included

| Area | Feature | API |
|:-----|:--------|:----|
| Requests | Routing by path and method, with a trailing `*` for prefixes, and `405` with an `Allow` header | `mado::Route {path, method, id}` |
| Requests | Query parameters and urlencoded form fields decoded into your buffers | `query_param()`, `mado::next_form_field()` |
| Requests | Only the headers you name, matched without regard to case; bodies in blocks after `100 Continue` | `header()`, `read_body()` |
| Requests | `408` on a stalled read or a slow request, `414` on a long request line, `431` on too many header lines, `505` on any version but HTTP/1.0 and 1.1 | automatic |
| Requests | Only the `Host` names you list, with or without a port, against DNS rebinding; a hook called with every response | `PolicyT::trusted_hosts()`, `PolicyT::on_response()` |
| Responses | Status lines with reason phrases, any header, CORS for listed origins, redirects, output gathered into writes of up to 1,460 bytes | `send_status()`, `send_header()`, `mado::send_cors_headers()`, `send_redirect()`, `mado::Reply` |
| Responses | RFC 9457 problem details: `application/problem+json` with a title, the status and a detail | `mado::send_problem()`, `mado::json_problem()` |
| Responses | `$NAME$` tags in a template read from a file; ArduinoJson documents with an exact `Content-Length` | `mado::send_template()`, `mado::send_json()` |
| Card | Pages and files from the card: `/` to `index.html`, a `.gz` copy next to each file, `Vary: Accept-Encoding`, a year of caching for hashed names and `no-cache` for the rest | `mado::serve_card_file()`, `mado::serve_card_path()` |
| Files | Files with a content type per extension; one byte range with `206` or `416` and `If-Range`, a weak ETag with `304`, and a download name | `mado::send_file()`, `mado::FileReply`, `mado::content_type_for()` |
| Files | URL paths decoded and checked; `.` and `..` segments, empty segments, backslashes and encoded NUL bytes are rejected | `mado::resolve_static_path()` |
| Files | Uploads checked against long names and a `Repr-Digest`, written to a temporary file and moved into place; sizes over 4 GiB refused | `mado::valid_card_name()`, `mado::parse_repr_digest()`, `mado::copy_file()`, `mado::replace_file()`, `mado::file_length()` |
| Network | DHCP with a timeout and a fixed fallback address; a random MAC created once and kept in EEPROM | `mado::begin_network()`, `mado::load_mac()` |
| Network | A restart when the cable or the chip comes back; `<name>.local` answered over mDNS | `mado::NetworkWatch`, `mado::MdnsResponder` |
| Storage | One JSON line per response in the shape pino-http writes, rotated past a size you set | `mado::SdLog`, `mado::write_request_log()` |
| Storage | Small counters as text files on SD; fixed records in EEPROM | `mado::read_count_file()`, `mado::write_count_file()`, `mado::EepromTable` |
| Auth | HTTP Basic credentials checked in constant time | `mado::check_basic_auth()` |
| Auth | HS256 JSON Web Tokens issued and verified; PBKDF2-HMAC-SHA256 password hashes | `mado::issue_token()`, `mado::verify_bearer()`, `mado::pbkdf2_sha256()` |
| Auth | Login locked for 15 minutes after five failures | `mado::LoginGuard` |
| Auth | SHA-256, HMAC-SHA256 and Base64url | `mado::Sha256`, `mado::HmacSha256`, `mado::Base64UrlEncoder`, `mado::Base64UrlDecoder` |
| REST | JSON request bodies read field by field with no document in RAM; escaped JSON strings | `mado::JsonReader`, `mado::json_write_string()` |
| REST | `{id}` read from the end of a path; a fixed table of records | `mado::parse_path_id()`, `mado::SlotTable` |
| REST | C++ routes and schemas generated from an OpenAPI 3.1 document | [`tools/madogen`](tools/madogen/cli.py) |

## Guide

### Routes and handlers

A route is `{path, method, id}`, and the table ends with an entry whose path is `nullptr`. Routes match in table order, so put specific paths before wildcards, and the card route `/*` last.

- An exact path such as `/api/status` matches only that path. The query string is ignored when matching.
- A path ending in `*`, such as `/api/files/*`, matches every path that starts with `/api/files/`.
- `mado::Method::kAny` accepts every method. A `kGet` route also answers `HEAD`, and the body is left out.
- A path that matches with the wrong method gets `405`. No match at all gets `404`.

`handle` returns `true` when the response is complete and the connection should close. Return `false` to keep it open, for example to stream updates later through `exchange.client()`. A W5100 has four sockets, so keep few connections open.

### The exchange

`mado::Exchange` is the request and the response for one connection.

| Request | Returns |
|:--------|:--------|
| `method()` | `mado::Method` |
| `path()` | the path as sent, without the query string |
| `query()` | the raw query string, empty when there is none |
| `query_param(name, out, size)` | `true` and the decoded value in `out` |
| `header(name)` | the captured value, or `nullptr` |
| `content_length()` | the declared body length |
| `read_body(buffer, size)` | bytes read, `0` at the end, `-1` on timeout or a closed connection |
| `remaining_body()` | body bytes not read yet |
| `client()` | a pointer to the client, for raw access |

| Response | Sends |
|:---------|:------|
| `send_status(code)` | `HTTP/1.1 <code> <reason>` and `Connection: close` |
| `send_header(F("Name"), value)` | one header; the value is a RAM string, a number, or `mado::FlashText(F("..."))` |
| `send_content_type(F("type"))` | `Content-Type` |
| `send_content_length(length)` | `Content-Length` |
| `send_cors_headers()` | `Access-Control-Allow-Origin: *` and the allowed methods and headers |
| `mado::send_cors_headers(exchange, origins)` | `Vary: Origin`, and the CORS headers only when the captured `Origin` is in the list |
| `send_location(F("/api/items/"), id)` | `Location: /api/items/<id>` |
| `status_code()` | the status sent so far, or `0` |
| `end_headers()` | the blank line; after it, a `HEAD` request drops the body |
| `send_empty(code)` | a complete response with no body |
| `send_redirect(F("/next"))` | `303 See Other` with `Location` |
| `print(...)`, `print_signed(...)`, `write(...)` | body text, numbers and bytes |

### Request headers

Name the headers you need when you create the server. Their values share a fixed pool, 48 bytes by default, and longer values are cut short. `Content-Length` and `Expect` are always read. A card route needs `Accept-Encoding` to pick the gzip copies.

```cpp
const char kAuthorization[] PROGMEM = "Authorization";
const char kAcceptEncoding[] PROGMEM = "Accept-Encoding";
const char* const kHeaderNames[] PROGMEM = {&kAuthorization[0], &kAcceptEncoding[0], nullptr};
mado::Server<EthernetClient, WebApp> web(&kRoutes[0], &kHeaderNames[0]);
```

### Query strings and forms

```cpp
#include <mado/next_form_field.h>

char name[24];
char value[24];
if (exchange.query_param(F("name"), &name[0], sizeof(name))) {
  exchange.print(&name[0]);
}
while (mado::next_form_field(exchange, &name[0], sizeof(name), &value[0], sizeof(value)) ==
       mado::FormStatus::kField) {
  exchange.print(&value[0]);
}
```

### Files on the card

```cpp
#include <mado/serve_card_file.h>

const bool served = mado::serve_card_file<FILE_READ>(exchange, sd_card);
const char kSpecPath[] PROGMEM = "/openapi.json";
const bool spec = mado::serve_card_path<FILE_READ>(exchange, sd_card, &kSpecPath[0]);
```

`serve_card_file()` resolves the request path on the card, answers `400` for a path that climbs out and `404` for a missing file or a folder, and sends the `.gz` copy next to the file when the browser accepts gzip. A name that is eight hex digits and an extension, such as `9f86d081.js`, is cached as `public, max-age=31536000, immutable`; every other file gets `no-cache`. `serve_card_path()` serves one fixed file, which is how `/api/openapi.json` reaches `/openapi.json`.

For downloads, ranges and conditional requests, pass a `mado::FileReply` to `mado::send_file()` with the size, type, gzip flag, ETag, download name and `Cache-Control` value. `mado::file_etag()` builds a weak ETag from the name, the size and a version that the sketch changes on every write and at boot, so a file replaced by one of the same size gets a new tag. A `Range` with `If-Range` gets the whole file, because a weak tag cannot prove the parts belong together. [`RestFiles`](examples/RestFiles/RestFiles.ino) uses all of it.

### Basic auth

```cpp
#include <mado/check_basic_auth.h>
#include <mado/send_auth_challenge.h>

if (!mado::check_basic_auth(exchange.header(F("Authorization")), F("YWRtaW46Y2hhbmdlLW1l"))) {
  return mado::send_auth_challenge(exchange, F("Madoguchi"));
}
```

The expected value is `user:password` in Base64, from `printf 'admin:change-me' | base64`.

> [!WARNING]
> The board has no TLS. Basic auth passwords, login passwords and bearer tokens cross the network in clear text. Keep the board on a network you trust, or behind a reverse proxy that terminates TLS.

### Templates

A template is a file on the card with `$NAME$` tags that code fills in while it streams the file:

```cpp
#include <mado/FileSource.h>
#include <mado/send_template.h>

struct PageTags {
  static bool render(WebExchange& exchange, const char* tag) {
    if (strcmp_P(tag, PSTR("TEMP")) == 0) {
      exchange.print(static_cast<uint32_t>(analogRead(A0)));
      return true;
    }
    return false;
  }
};

FsFile page = sd_card.open("/status.html", FILE_READ);
mado::FileSource<FsFile> source(page);
mado::send_template<WebExchange, PageTags>(exchange, source);
```

Tags are up to 16 letters, digits or underscores. An unknown tag is sent as written, and `$$` before a tag keeps one `$`.

### JSON with ArduinoJson

```cpp
#include <ArduinoJson.h>
#include <MadoguchiJson.h>

JsonDocument document;
document["uptime_ms"] = millis();
mado::send_json(exchange, mado::http::kOk, document);
```

`MadoguchiJson.h` is the only file that includes ArduinoJson, so a sketch that does not include it never compiles it.

### Limits

Buffer sizes and timeouts come from a type passed as the third template argument. Declare every field, and the handler then takes `mado::Exchange<EthernetClient, BigLimits>&`.

```cpp
struct BigLimits {
  static constexpr uint16_t kLineBufferSize = 256U;
  static constexpr uint16_t kOutputBufferSize = 512U;
  static constexpr uint8_t kHeaderPoolSize = 96U;
  static constexpr uint8_t kHeaderNameSize = 32U;
  static constexpr uint8_t kMaxCapturedHeaders = 8U;
  static constexpr uint8_t kMaxHeaderLines = 48U;
  static constexpr uint8_t kMaxRoutes = 32U;
  static constexpr uint16_t kReadTimeoutMs = 1000U;
  static constexpr uint32_t kMaxPollsPerRead = 100000UL;
  static constexpr uint16_t kRequestTimeoutMs = 5000U;
  static constexpr uint16_t kMaxDrainBytes = 1024U;
  static constexpr uint16_t kFileChunkSize = 512U;
};
mado::Server<EthernetClient, WebApp, BigLimits> web(&kRoutes[0], nullptr);
```

| Field | Default | Meaning |
|:------|--------:|:--------|
| `kLineBufferSize` | 128 | longest request line: method, path and query together |
| `kOutputBufferSize` | 32 | bytes gathered before a network write, up to 1,460; a larger write goes out in one call |
| `kHeaderPoolSize` | 48 | room for all captured header values |
| `kHeaderNameSize` | 32 | longest header name that can be captured |
| `kMaxCapturedHeaders` | 8 | names in the header list |
| `kMaxHeaderLines` | 48 | header lines accepted before `431` |
| `kMaxRoutes` | 32 | routes scanned in the table |
| `kReadTimeoutMs` | 1000 | wait for the next byte before `408` |
| `kMaxPollsPerRead` | 100000 | upper bound on polls per read |
| `kRequestTimeoutMs` | 5000 | time for the request line and headers, and again for the body, before `408` |
| `kMaxDrainBytes` | 1024 | unread body bytes dropped before closing; a longer body is left unread |
| `kFileChunkSize` | 32 | bytes read from a file per step of a file response |

A fourth template argument, the policy, lists the trusted hosts and receives each response. The default trusts any `Host` up to 255 bytes and ignores the hook.

```cpp
struct BoardPolicy {
  static constexpr bool kTrustedHostsOnly = true;
  static const char* const* trusted_hosts() {
    static const char* const kHosts[] = {"192.168.1.177", "board.local", nullptr};
    return &kHosts[0];
  }
  static void on_response(const mado::RequestLine&, const mado::ResponseSummary&) {}
};
mado::Server<EthernetClient, WebApp, mado::DefaultLimits, BoardPolicy> web(&kRoutes[0], nullptr);
```

### Network

```cpp
#include <mado/MdnsResponder.h>
#include <mado/begin_network.h>
#include <mado/load_mac.h>

const char kHostName[] PROGMEM = "madoguchi";
uint8_t mac[mado::kMacSize] = {};
mado::MdnsResponder<EthernetUDP, IPAddress> mdns(&kHostName[0]);

void setup() {
  mado::load_mac(EEPROM, 8U, mac, micros());
  mado::begin_network(Ethernet, &mac[0], IPAddress(192, 168, 1, 177), 10000UL);
  mdns_socket.beginMulticast(IPAddress(224, 0, 0, 251), mado::kMdnsPort);
}
```

`load_mac()` writes a random locally administered MAC to seven bytes of EEPROM on first boot and reads it back after. `begin_network()` returns `false` when DHCP did not answer in time and the fixed address is in use. In `loop()`, call `mdns.poll(mdns_socket, Ethernet.localIP())` so `http://madoguchi.local` resolves, `Ethernet.maintain()` to renew the lease, and `mado::NetworkWatch::update()` with the chip and link status; it returns `mado::NetworkAction::kRestart` when the network must be started again. [`RestApi.ino`](examples/RestApi/RestApi.ino) does all of it.

### Request log

`mado::SdLog` appends one JSON line per response, in the shape pino-http writes, and moves the file to a second name past a size you set:

```json
{"level":30,"boot":3,"uptime":1500,"hostname":"madoguchi","req":{"id":12,"method":"PUT","url":"/api/items/2"},"res":{"statusCode":200},"responseTime":9,"msg":"request completed"}
```

`level` is 30 for a response below 500 and 50 for a server error. `boot` counts restarts and `uptime` is `millis()`, since the board has no clock. Call `log.request()` from the policy's `on_response()`, and `log.event()` for anything else, such as a login. RestApi writes `/LOG.TXT`, keeps the previous 64 KiB in `/LOG.OLD`, and records no user name or address. Read it with any NDJSON tool:

```sh
curl -s -H "Authorization: Bearer $TOKEN" http://madoguchi.local/api/log | jq -c 'select(.level >= 40)'
```

## REST API with OpenAPI and JWT

[`RestApi`](examples/RestApi/RestApi.ino) is a complete authenticated API with a W5x00 shield:

| Path | Serves |
|:-----|:-------|
| `/` | a page that signs in and lists, adds, toggles and deletes items, from the card |
| `/docs/` | Swagger UI for the board's own API, from the card |
| `GET /api/openapi.json` | the OpenAPI 3.1 document, from the card |
| `POST /api/auth/login` | a bearer token for a valid user name and password |
| `GET /api/items`, `GET /api/items/{id}` | the items, public |
| `POST /api/items`, `PUT`, `PATCH`, `DELETE /api/items/{id}` | changes, with a bearer token |
| `GET /api/log`, `DELETE /api/log` | the request log as JSON lines, with `Range` for its end, and clearing it, with a bearer token |

[`RestAuth`](examples/RestAuth/RestAuth.ino) shows login and one protected route. [`RestGet`](examples/RestGet/RestGet.ino), [`RestPost`](examples/RestPost/RestPost.ino), [`RestPut`](examples/RestPut/RestPut.ino), [`RestPatch`](examples/RestPatch/RestPatch.ino) and [`RestDelete`](examples/RestDelete/RestDelete.ino) each show one verb with no authentication. [`RestFiles`](examples/RestFiles/RestFiles.ino) lists, downloads, uploads with a progress bar and deletes files on the SD card, with no authentication, so it belongs on a trusted network. Each keeps its application code in a header next to the sketch, so the host tests run the code the board runs, and answers `503` with a problem document when the card is missing.

### Wiping and formatting a card

[`RestFormat`](examples/RestFormat/RestFormat.ino) prepares a card for every other example. It answers JSON only, with no pages and no login, so keep it on a trusted network. It erases everything on the card.

| Route | Does |
|:------|:-----|
| `GET /api/card` | the capacity in MiB, the kind, `SDSC`, `SDHC` or `SDXC`, and the file system the format writes |
| `POST /api/card/format` | starts the wipe; the body repeats the capacity, `{"capacity_mib":30436}`, so a request meant for one card never wipes another; `202` with a `Location`, `409` while a wipe runs, `422` for another capacity |
| `GET /api/card/format` | the phase, its percent, and a detail that names a failure |

The phases are `fill-zero`, `check-zero`, `fill-one`, `check-one`, `format`, `mount` and `done`, or `failed`. The board writes 0x00 to every sector and reads every sector back, then does the same with 0xFF, a few sectors per pass of `loop()`, so the API keeps answering. It then formats the card with SdFat's formatters, picking the file system by capacity, and mounts it to confirm the result:

| Sectors | File system | Card |
|:--------|:------------|:-----|
| under 4,194,304, up to 2 GiB | FAT16 | SDSC |
| up to 67,108,864, 32 GiB | FAT32 | SDHC |
| more | exFAT | SDXC |

The wipe reads and writes the whole card twice each, so its time grows with the card's size and SPI speed. The format and the mount block the board for a few seconds.

```sh
curl -s http://192.168.1.177/api/card
curl -s -H 'Content-Type: application/json' -d '{"capacity_mib":30436}' http://192.168.1.177/api/card/format
curl -s http://192.168.1.177/api/card/format
```

### Generating the API code

Each REST sketch keeps an `openapi.json`. [`tools/madogen`](tools/madogen/cli.py) turns it into `generated_api.h`, which is committed, so the Arduino IDE builds the sketch with no Python installed. The generator uses only the Python standard library.

```sh
python3 -m tools.madogen generate examples/RestApi
python3 -m tools.madogen check examples/RestApi
```

`generate` writes:

- route ids from each `operationId`, the route table from `paths`, a route for `/api/openapi.json` and a last route `/*` for the card;
- `route_needs_token()`, from each operation's `security`;
- `route_path_id()`, which reads the `{id}` at the end of a path;
- a struct and a field reader for every schema in `components.schemas`, checking each `maxLength`, `minimum` and `maximum`, and a JSON writer for each schema a response uses or that sets `x-mado-writer: true`;
- `read_<schema>_body()`, which checks `Content-Type` and length, reads the body and names the first missing required property in a `mado::Problem`, and `<operation>_url()` for each `GET` by id;
- `send_card()`, which serves the spec and every other path from the card, and answers `503` when there is no card.

`check` fails when `generated_api.h` no longer matches its sources, or when `arduino_secrets.h` holds a real key. CI runs it on all nine examples.

The generator accepts a subset of OpenAPI 3.1 and names the JSON pointer of anything outside it:

- paths start with `/api/`, and only the last segment may be a parameter;
- request bodies are `application/json` referencing a component schema, or `application/octet-stream`;
- `in: header` parameters become captured request headers;
- an optional top-level `x-mado-limits` sets `outputBufferSize` from 8 to 1,460 and `fileChunkSize` up to 1,024;
- schemas are flat objects with `additionalProperties: false`, holding strings with `maxLength`, integers with `minimum` and `maximum`, and booleans;
- security is `[]` or `[{"bearerAuth": []}]`, with `bearerAuth` an HTTP bearer JWT scheme;
- at most 31 operations.

`python3 -m tools.madogen sri FILE` prints the Subresource Integrity hash of a file, for pinning a new CDN file in the kit.

### Signing in

The committed `arduino_secrets.h` holds an all-zero key, and until it is replaced, login answers `503` and no token is accepted. Create real secrets once per board:

```sh
python3 -m tools.madogen secrets examples/RestApi --user admin
```

It asks for a password of 12 to 64 characters and writes a random 32-byte signing key, a random salt and the password's PBKDF2-HMAC-SHA256 hash. Never commit that file: `check`, which CI runs, fails when it holds a real key.

- Tokens are HS256 JWTs. The board compares the token header byte for byte, so `alg: none` and every other algorithm fail, and it compares signatures in constant time.
- A token lasts 15 minutes of uptime and ends when the board restarts, because each start bumps a session counter in EEPROM.
- Five failed logins in a row lock login for 15 minutes and answer `429` with `Retry-After`.
- Passwords use PBKDF2 because Argon2id and bcrypt need more RAM than the board has. Tune `--iterations` so a login takes about one second.
- The pages keep the token in memory only, never in browser storage, so a reload signs out.

The pages load Pico CSS and Swagger UI from public CDNs, so the browser needs internet access for styling and for `/docs/`. The API itself works offline.

The generated `Limits` raises the header pool to 240 bytes, which holds a bearer token next to `Accept-Encoding`, `If-None-Match` and `Content-Type`. Request bodies over 256 bytes get `413`, and every API error carries an `application/problem+json` body such as `{"title":"Unprocessable Content","status":422,"detail":"missing required property: on"}`.

## Development

Host tests use CMake and googletest, with a small Arduino shim, a fake client, a fake SD card that matches names without regard to case, as FAT does, and a fake sector device for the format example.

| Suite | Command | Covers |
|:------|:--------|:-------|
| Library and examples | `cmake -S tests -B build -DMADO_COVERAGE=ON && cmake --build build && ctest --test-dir build --output-on-failure` | 1,182 tests: an RFC 9112 conformance table, parsing, routing, responses, card files, ranges, the network helpers, the SD log, crypto, tokens, the examples' application code, and a replay of each fuzz corpus |
| Coverage | `gcovr build --root . --filter src/ --merge-lines --txt` | line and function coverage of `src/`, gated at 95 percent in CI |
| Generator and gates | `python3 -m pytest -q tests/madogen tests/assertion_density tests/size_gate tests/throughput` | 91 tests for `madogen`, the assertion density gate, the size gate and the throughput tool |
| Pages | `pnpm run check && pnpm run test && pnpm run build` | Biome, TypeScript, html-validate, 106 Vitest tests with a 95 percent coverage gate, and every card build |
| Browser | `pnpm exec playwright install chromium && pnpm run build && pnpm run e2e` | 32 Playwright tests: every page and docs page at 320 px and desktop width |
| Throughput | `python3 tools/throughput.py --download http://madoguchi.local/ --upload http://madoguchi.local/api/files/BENCH.BIN` | download and upload speed against a board, printed as JSON |
| Generated code | `python3 -m tools.madogen check examples/Rest*` | `generated_api.h` matches its sources and no real key is committed |
| Fuzzing | `cmake -S tests -B fuzz -DMADO_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++ && cmake --build fuzz && fuzz/mado_fuzz_request tests/fuzz/corpus/request` | libFuzzer with AddressSanitizer and UBSan on the server, the RestApi and RestFiles apps, the JSON reader and the text decoders |
| Size budget | `python3 tools/size_gate.py --fqbn arduino:renesas_uno:minima examples/RestApi` | flash and RAM against the board's budget, with zero compiler warnings |

CI also runs clang-format, clang-tidy, cppcheck 2.22.0 with `--enable=all`, the assertion density gate and arduino-lint in strict mode, with zero warnings allowed. The exact commands are in [`.github/workflows/ci.yml`](.github/workflows/ci.yml), and the coding rules are in [`AGENTS.md`](AGENTS.md).

## License

[MIT](LICENSE). Use, copy, change and ship it in any project, commercial ones included, as long as the copyright notice stays with the code.
