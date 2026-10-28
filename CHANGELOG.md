# Changelog

## 1.0.0

First release.

### Added

- An HTTP/1.1 server for the Uno R4 Minima, templated on the client type, with fixed buffers sized by a `Limits` type and no heap allocation. Routes, header names and constant strings live in flash.
- Strict request parsing per RFC 9112: one `Host` on HTTP/1.1, `400` on a repeated `Content-Length`, `501` on `Transfer-Encoding`, `505` on versions other than 1.0 and 1.1, `408` on a stalled read or a slow request, `414` and `431` on oversized input, and a drain of the unread body before the connection closes.
- A server policy with a trusted host list against DNS rebinding and an `on_response()` hook that receives a `mado::ResponseSummary`.
- Responses: status lines including `202 Accepted` and `409 Conflict`, headers, CORS for listed origins, redirects, `mado::Reply`, RFC 9457 problem documents, `$NAME$` templates streamed from a file or any source with `next()`, and ArduinoJson documents through the opt-in `MadoguchiJson.h`.
- Pages from the SD card through SdFat 2.3.0, with long file names on FAT16, FAT32 and exFAT: `mado::serve_card_file()` maps `/` to `index.html`, prefers the `.gz` copy next to a file, sends `Vary: Accept-Encoding`, and caches hashed names for a year; `mado::serve_card_path()` serves one fixed file. No HTML, CSS, script or image lives in C++ code.
- Files: content types, one byte range with `206` or `416`, `If-Range`, weak ETags that mix in a write version, `304`, downloads with `Content-Disposition`, long-name checks, sizes over 4 GiB refused, `Repr-Digest` checks and replacement through a temporary file.
- Authentication: HTTP Basic, HS256 JSON Web Tokens, PBKDF2-HMAC-SHA256 password hashes, a login lockout, SHA-256, HMAC-SHA256 and Base64url.
- Network: DHCP with a timeout and a fixed fallback address, a random locally administered MAC kept in EEPROM, a restart when the cable or the chip comes back, and an mDNS responder for `<name>.local`.
- Storage: `mado::SdLog`, which appends one JSON line per response in the shape pino-http writes and rotates the file, counters kept as text files on SD, and `mado::EepromTable`.
- `tools/madogen`, which turns an OpenAPI 3.1 document into C++: routes, schema structs, readers and writers, body readers that name the first missing property, and a `send_card()` that serves the spec and every other path from the card, which `x-mado-card: false` leaves out. It also checks generated code against its sources, provisions secrets and prints SRI hashes.
- An `SD Card` web project per example, built in one pnpm workspace: semantic HTML, strict TypeScript, classless Pico CSS with light and dark mode, Vitest tests, and a Vite build that writes `.gz` copies, a Content-Security-Policy per page and the OpenAPI document as `openapi.json`, and checks every name against what the board serves. Pico CSS and Swagger UI load from CDNs with Subresource Integrity.
- `@madoguchi/kit`, the shared web code: CDN pins, the theme toggle, fetch helpers for RFC 9457 problems, an in-memory bearer session and the Vite configuration.
- `tools/throughput.py`, which measures download and upload speed against a board.
- Nine examples: RestGet, RestPost, RestPut, RestPatch and RestDelete, one verb each; RestAuth, login and a protected route; RestFiles, an SD file manager with upload progress; RestApi, an authenticated REST API with a site, Swagger UI and a request log; and RestFormat, a JSON API that fills the card with 0x00 and 0xFF, verifies both passes, and formats it as FAT16, FAT32 or exFAT by capacity.
- Host tests with googletest, an RFC 9112 conformance table, libFuzzer targets for the server, the REST applications, the JSON reader and the text decoders, generator and page tests, Playwright tests of every page at 320 px and desktop width, and CI that fails any build over the board's flash or RAM budget.
