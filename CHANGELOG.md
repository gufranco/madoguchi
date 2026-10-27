# Changelog

## 1.0.0

First release.

### Added

- An HTTP/1.1 server for the Uno R3 and the Uno R4 Minima, templated on the client type, with fixed buffers sized by a `Limits` type and no heap allocation. Routes, header names and constant strings live in flash.
- Strict request parsing per RFC 9112: one `Host` on HTTP/1.1, `400` on a repeated `Content-Length`, `501` on `Transfer-Encoding`, `505` on versions other than 1.0 and 1.1, `408` on a stalled read or a slow request, `414` and `431` on oversized input, and a drain of the unread body before the connection closes.
- A server policy with a trusted host list against DNS rebinding and an `on_response()` hook that receives a `mado::ResponseSummary`.
- Responses: status lines, headers, CORS for listed origins, redirects, `mado::Reply`, RFC 9457 problem documents, templates in flash, and ArduinoJson documents through the opt-in `MadoguchiJson.h`.
- Files: SD and flash files with gzip copies, content types, one byte range with `206` or `416`, `If-Range`, weak ETags that mix in a write version, `304`, downloads with `Content-Disposition`, an optional `Cache-Control` on the small `send_file()` used by WebServerSD and BlinkLed, 8.3 name checks, `Repr-Digest` checks and replacement through a temporary file.
- Authentication: HTTP Basic, HS256 JSON Web Tokens, PBKDF2-HMAC-SHA256 password hashes, a login lockout, SHA-256, HMAC-SHA256 and Base64url.
- Network: DHCP with a timeout and a fixed fallback address, a random locally administered MAC kept in EEPROM, a restart when the cable or the chip comes back, and an mDNS responder for `<name>.local`.
- Storage: `mado::SdLog`, which appends one JSON line per response in the shape pino-http writes and rotates the file, counters kept as text files on SD, and `mado::EepromTable`.
- `tools/madogen`, which turns an OpenAPI 3.1 document and a folder of web files into C++: routes, schema structs, readers and writers, body readers that name the first missing property, minified and fingerprinted assets with precomputed headers, and the document itself. It also checks generated code against its sources, provisions secrets and prints SRI hashes.
- `tools/throughput.py`, which measures download and upload speed against a board.
- Seventeen examples, from a single page on the Uno R3 to RestApi, an authenticated REST API with a site, Swagger UI and a request log, and RestFiles, an SD file manager with upload progress, on the Uno R4 Minima.
- Host tests with googletest, an RFC 9112 conformance table, libFuzzer targets for the server, the REST applications, the JSON reader and the text decoders, generator and page-script tests, and CI that fails any build over a board's flash or RAM budget.
