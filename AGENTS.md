# Agent Instructions

## Hard Rules

### The library MUST build and run on the Uno R4 Minima

Madoguchi supports exactly one board: the Arduino Uno R4 Minima, a Renesas RA4M1, through a W5100, W5500, or ENC28J60 shield and an SD card. No other board is a target: not the Uno R3, not the Uno R4 WiFi, not the UNO Q. The owner dropped the Uno R3 on 2026-10-27, after FormAndQuery, JsonApi and EthernetENC measured 32,746, 37,964 and 36,932 bytes with an SD card against the R3's 32,256, and removed the examples written for it. The examples are the eight REST examples and RestFormat, which the owner asked for on 2026-10-27: it wipes a card with 0x00 and 0xFF, verifies both passes and formats it for SdFat, answers JSON only, and has no `SD Card` folder because it loads nothing into a browser.

- Uno R4 Minima budget: 262,144 bytes of flash and 32,768 bytes of SRAM.
- Every example MUST fit the Uno R4 Minima next to Ethernet 2.0.2 or EthernetENC 2.0.5 and SdFat 2.3.0.
- Card access MUST go through SdFat 2.3.0, `SdFs` and `FsFile`, not the Arduino SD library: SD 1.3.0 reads only 8.3 names, and SdFat reads long names on FAT16, FAT32 and exFAT. The owner chose SdFat on 2026-10-27.
- A sketch MUST include `<SdFat.h>` in its own include block after every Madoguchi and app header. The R4 core defines `F()` as a function, so SdFat's `#ifndef F` guard passes and it defines `F(str)` as a plain string, which breaks every `F()` that follows.
- Prefer opt-in by reference, so the linker drops what a sketch never calls, over a compile-time switch.
- The library MUST NOT allocate on the heap. Fixed buffers only, sized by compile-time constants.
- Constant strings and tables MUST live in flash: `F()`, `PROGMEM`, and `pgm_read_byte`.
- CI MUST build every example for the Uno R4 Minima and fail when flash or SRAM use crosses its budget. CI MUST NOT build for any other board.
- A change that raises flash or SRAM use MUST state the before and after numbers in its pull request.

### No asset in C or C++ code

No HTML, CSS, JavaScript, image, font, document or OpenAPI file MAY exist inside C or C++ code: not as a string, not as a byte array, not generated into a header. The board MUST read every file a client asks for from the SD card and serve it. Responses that code builds from data, such as JSON documents, RFC 9457 problems, and short `text/plain` messages, are data and MAY be written by code. The owner set this rule on 2026-10-27.

- Every example that gives a client anything to load MUST keep it in an `SD Card` folder inside the example.
- Each `SD Card` folder MUST be a separate web project, built by its own tooling into the files copied to the card: HTML that follows current standards, light and dark mode, TypeScript in place of JavaScript, and classless Pico CSS.
- The build MUST write only names the board serves, 1 to 64 printable ASCII characters per segment with none of `" * / : < > ? \ | %` and a path under 128 bytes with its `.gz` suffix, MUST put a gzip copy named `<file>.gz` next to each file that compresses, and MUST NOT require any step on the board.

### Front-end dependencies come from a CDN

Every third-party front-end dependency, Pico CSS and Swagger UI included, MUST load from a public CDN with an exact version and a Subresource Integrity hash. No third-party front-end file MAY be copied to the SD card or served by the board. The owner set this rule on 2026-10-27.

### NASA/JPL coding rules

All code in `src/` and [`examples/`](examples) MUST follow the JPL Institutional Coding Standard for the C Programming Language, JPL D-60411 version 1.0 of 2009-03-03, levels LOC-1 to LOC-4, and the Power of Ten rules by Gerard J. Holzmann, IEEE Computer, June 2006. Where the two differ, the stricter rule applies, and every JPL `should` rule is applied as a `shall`: the owner asked on 2026-10-26 for the strictest rules possible. The library is C++ for the Arduino toolchain, so each rule below is stated in its C++ form. Rule numbers point to the source: `JPL n` for D-60411, `P10 n` for the Power of Ten.

#### Language compliance

- Code MUST stay inside the language definition, C++11 as built by the oldest supported core, with no reliance on undefined or unspecified behavior. `#pragma` MUST NOT be used. JPL 1.
- Code MUST compile with every warning enabled at the most pedantic level the toolchain offers, with zero warnings: arduino-cli `--warnings all`, and `-Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wconversion -Wmissing-declarations -Werror` in the host build. JPL 2, P10 10.
- Code MUST pass at least two static analyzers with zero findings on every push: cppcheck with `--enable=all`, and clang-tidy with the `bugprone-*`, `cert-*`, `cppcoreguidelines-*`, `misc-*`, `performance-*`, and `readability-*` groups. A confused analyzer is answered by rewriting the code until it is trivially valid, never by a suppression. JPL 2, P10 10.

#### Predictable execution

- Every loop MUST have a fixed upper bound that a static checker can prove: a constant iteration cap. A loop that waits on time or on the network MUST also carry an iteration cap. The library has no non-terminating loop; `loop()` belongs to the Arduino core. JPL 3, P10 2.
- Recursion, direct or indirect, MUST NOT be used. JPL 4, P10 1.
- Dynamic memory MUST NOT be used: no `malloc`, `calloc`, `realloc`, `free`, `new`, `delete`, `alloca`, `String`, or any container that allocates. This is stricter than JPL 5 and P10 3, which allow allocation during initialization.
- `goto`, `setjmp`, and `longjmp` MUST NOT be used. JPL 11, P10 1.
- In an enumerator list, `=` MUST NOT initialize any member other than the first, unless every member is initialized. JPL 12.
- Task delays MUST NOT be used for synchronization: no `delay()` in the library to wait for data. JPL 7.
- The library MUST NOT be called from an interrupt handler, and it shares no data with one, so JPL 6, 8, and 9 on tasks, IPC, ownership, and locks have no subject here. A future change that adds shared state MUST apply them.
- Every internal buffer MUST be followed by a barrier pattern that is checked after each request; a broken barrier fails the request and is reported. JPL 10.

#### Defensive coding

- Data objects MUST be declared at the smallest possible scope. No inner declaration MAY hide an outer one. Parameters and data that do not change MUST be `const` or `constexpr`. `extern` declarations MUST live in a header included by both the definer and every user. JPL 13, P10 6.
- The return value of every non-void function MUST be checked or used, or explicitly cast to `void` when irrelevant. JPL 14, P10 7.
- Every public function MUST validate its parameters on entry; every other function's parameters MUST be validated by the function or by its caller. JPL 15, P10 7.
- Assertions MUST average at least two per function, and every function longer than 10 lines MUST have one. An assertion is a side-effect-free Boolean test through `mado::check`. On failure the function MUST take an explicit recovery action, such as returning an error to its caller. An assertion that a checker can prove always true or always false is a violation. JPL 16, P10 5.
- Fixed-width types from `<stdint.h>` MUST be used in place of `int`, `short`, `long`, and `char` for numbers. Overrides of Arduino virtual functions keep the core's signatures. JPL 17.
- In compound expressions with more than one operator, the order of evaluation MUST be explicit with parentheses. JPL 18.
- A Boolean expression MUST have no side effects. Call first, store the result in a `const`, then test it. JPL 19.

#### Code clarity

- The preprocessor MUST be limited to `#include` and include guards. Token pasting, variadic macros, and recursive macros MUST NOT be used. JPL 20, P10 8.
- Conditional compilation MUST be limited to include guards. JPL 20, P10 8.
- Macros MUST NOT be defined inside a function or a block. `#undef` MUST NOT be used. JPL 21, 22.
- `#else`, `#elif`, and `#endif` MUST sit in the same file as their `#if`. JPL 23.
- One statement or declaration per line; the three clauses of a `for` header are the only exception. JPL 24.
- Functions MUST be at most 60 lines and take at most 6 parameters. The repository's own limit of 30 lines is stricter and applies. JPL 25, P10 4.
- A declaration MUST have at most two levels of indirection, and an expression at most one level of dereference per object. Dereferences MUST NOT be hidden in macros or typedefs. JPL 26, 27, 28, P10 9.
- Function pointers MUST NOT be used at all: not stored, not passed, not cast, not taken as template arguments. P10 9 is stricter than JPL 29 and 30, and applies.
- The library MUST NOT declare virtual functions and MUST NOT inherit from `Print`, `Stream`, or `Client`. The server is a template on the sketch's client type, so every call into the transport names a concrete type. Virtual dispatch inside third-party drivers such as Ethernet is outside the library and out of its reach. P10 9.
- Route handlers MUST be reached through a static member function of a sketch-defined type, given as a template argument, that switches on a route id. The call graph stays static and acyclic. P10 1, 9.
- `#include` directives MUST come before any code or declaration in a file. JPL 31.

#### Out of reach

- JPL LOC-5 and LOC-6 are MISRA-C:2004 rules, omitted from the public JPL document for copyright, so they are not in this file. cppcheck's MISRA checks MAY be added when a licensed rule text is available.

#### Allowed preprocessor use

- Include guards are the only conditional compilation and the only macros in `src/`. Buffer sizes and timeouts come from a `Limits` type passed as a template argument, never from `#define` overrides.
- Assertions are calls to the inline function `mado::check`, never a macro.

#### Analyzer checks that cannot apply

The library is freestanding C++11 and includes no C++ standard library header, so it has no `std::array`, `std::span`, or standard library containers. These clang-tidy checks demand those types, so they are disabled in `.clang-tidy` and nowhere else:

- `cppcoreguidelines-avoid-c-arrays`, `cppcoreguidelines-pro-bounds-constant-array-index`, `cppcoreguidelines-pro-bounds-pointer-arithmetic`: fixed buffers need raw arrays and indexing.
- `cppcoreguidelines-pro-type-reinterpret-cast`: Arduino's `F()` yields a `const __FlashStringHelper*`, which must be cast to read flash. One function in the library performs that cast.
- `modernize-*`: those checks propose C++14 and later forms, and the library stays inside C++11.

## Sources

- JPL D-60411, external-distribution edition, cleared 2009-03-04, CL#09-0763. Copy read on 2026-10-26 from a public mirror, SHA-256 `30c12c7aed3d02c88e3ac51fce3fe561a1c6ee7233957e5ec60bae635c44e0b5`.
- "The Power of Ten: Rules for Developing Safety Critical Code", from https://spinroot.com/gerard/pdf/P10.pdf, read on 2026-10-26, SHA-256 `2217f41df961b9ed25cc072700b6332d6193799ed09d83be48bf791d05025f54`.
