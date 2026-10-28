// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef FILES_APP_H
#define FILES_APP_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/DecodeMode.h>
#include <mado/FileLength.h>
#include <mado/FileReply.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/Sha256.h>
#include <mado/check.h>
#include <mado/constant_time_equal.h>
#include <mado/content_type_for.h>
#include <mado/copy_file.h>
#include <mado/file_etag.h>
#include <mado/file_length.h>
#include <mado/flash_address.h>
#include <mado/http_status.h>
#include <mado/kSha256Size.h>
#include <mado/parse_repr_digest.h>
#include <mado/send_file.h>
#include <mado/send_problem.h>
#include <mado/url_decode.h>
#include <mado/valid_card_name.h>
#include <stdint.h>
#include <string.h>

#include "generated_api.h"

constexpr uint8_t kFilesListMax = 32U;
constexpr uint32_t kFilesMaxUpload = 8388608UL;
constexpr uint8_t kFilesPathSize = mado::kCardNameSize + 1U;
constexpr uint8_t kFilesLocationSize = 208U;
constexpr int32_t kFilesMaxListedSize = 0x7FFFFFFFL;
constexpr uint16_t kFilesChunkSize = 512U;
constexpr uint32_t kFilesMaxReads = kFilesMaxUpload + 1UL;
const char kFilesTemporary[] PROGMEM = "/UPLOAD.TMP";

template <typename ClientT, typename SdT, typename FileT, uint16_t kReadMode, uint16_t kWriteMode>
struct FilesApp {
  using Exchange = mado::Exchange<ClientT, rest_files::Limits>;

  struct State {
    SdT* sd;
    uint8_t chip_select;
    bool ready;
    uint32_t version;
  };

  static State& state() {
    static State instance = {nullptr, 0U, false, 0U};
    return instance;
  }

  static void begin(SdT& sd, const uint8_t chip_select, const uint32_t seed) {
    state() = State{&sd, chip_select, false, seed};
    state().ready = sd.begin(chip_select);
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    const bool known = mado::check(route <= rest_files::kCardFile);
    if (!known) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    const bool ready = card_ready();
    if ((route == rest_files::kOpenApiDocument) || (route == rest_files::kCardFile)) {
      return rest_files::send_card<kReadMode>(exchange, ready ? state().sd : nullptr, route);
    }
    if (!ready) {
      return mado::send_problem(exchange, mado::Problem{mado::http::kServiceUnavailable,
                                                        F("the SD card is not available")});
    }
    switch (route) {
      case rest_files::kListFiles: return list_files(exchange);
      case rest_files::kGetFile: return get_file(exchange);
      case rest_files::kPutFile: return put_file(exchange);
      case rest_files::kDeleteFile: return delete_file(exchange);
      default: return exchange.send_empty(mado::http::kNotFound);
    }
  }

 private:
  static bool card_ready() {
    const bool wired = mado::check(state().sd != nullptr);
    if (wired && (!state().ready)) {
      state().ready = state().sd->begin(state().chip_select);
      changed(state().ready);
    }
    return wired && state().ready;
  }

  static void changed(const bool happened) {
    const uint32_t step = happened ? 1U : 0U;
    state().version = state().version + step;
  }

  static bool bad_name(Exchange& exchange) {
    return mado::send_problem(
        exchange,
        mado::Problem{
            mado::http::kBadRequest,
            F("file names are 1 to 64 printable ASCII characters without \\ / : * ? \" < > | %")});
  }

  static bool missing(Exchange& exchange) {
    return mado::send_problem(exchange,
                              mado::Problem{mado::http::kNotFound, F("no file has this name")});
  }

  static bool file_path(const Exchange& exchange, char (&path)[kFilesPathSize]) {
    const char* const prefix = mado::flash_address(rest_files::get_file_url());
    const size_t skip = strlen_P(prefix);
    const char* const full = exchange.path();
    const bool prefixed = mado::check(strlen(full) >= skip) && (strncmp_P(full, prefix, skip) == 0);
    const char* const name = prefixed ? &full[skip] : "";
    path[0] = '/';
    const int16_t length = mado::url_decode(name, static_cast<uint16_t>(strlen(name)), &path[1],
                                            kFilesPathSize - 1U, mado::DecodeMode::kPath);
    return (length > 0) && mado::valid_card_name(&path[1]);
  }

  static bool list_files(Exchange& exchange) {
    char root_path[2] = {'/', '\0'};
    FileT root = state().sd->open(&root_path[0]);
    if (!root) {
      return mado::send_problem(exchange, mado::Problem{mado::http::kServiceUnavailable,
                                                        F("the SD card has no root folder")});
    }
    mado::Reply<Exchange> reply(exchange);
    bool written = reply.status(mado::http::kOk).json().no_store().end();
    written = written && (exchange.print(F("[")) > 0U);
    uint8_t listed = 0U;
    for (uint8_t i = 0U; written && (i < kFilesListMax); ++i) {
      FileT entry = root.openNextFile();
      if (!entry) {
        break;
      }
      rest_files::FileEntry item = {};
      const bool named = describe(entry, item);
      entry.close();
      if (named) {
        written = ((listed == 0U) || (exchange.print(F(",")) > 0U)) &&
                  rest_files::write_file_entry(exchange, item);
        listed = static_cast<uint8_t>(listed + 1U);
      }
    }
    root.close();
    return written && (exchange.print(F("]")) > 0U);
  }

  static bool describe(FileT& entry, rest_files::FileEntry& item) {
    char name[mado::kCardNameSize + 1U] = {};
    const size_t length = entry.getName(&name[0], sizeof(name));
    const bool named = mado::check(length < sizeof(name)) && (length > 0U) &&
                       (length < sizeof(item.name)) && mado::valid_card_name(&name[0]);
    static_cast<void>(memcpy(&item.name[0], &name[0], named ? length : 0U));
    const uint64_t size = entry.size();
    const uint64_t listed = static_cast<uint64_t>(kFilesMaxListedSize);
    item.size = static_cast<int32_t>((size > listed) ? listed : size);
    item.directory = entry.isDirectory();
    return named;
  }

  static bool get_file(Exchange& exchange) {
    char path[kFilesPathSize] = {};
    if (!file_path(exchange, path)) {
      return bad_name(exchange);
    }
    FileT file = state().sd->open(&path[0], kReadMode);
    if ((!file) || file.isDirectory()) {
      return missing(exchange);
    }
    const mado::FileLength length = mado::file_length(file);
    if (!length.fits) {
      file.close();
      return mado::send_problem(exchange, mado::Problem{mado::http::kInternalServerError,
                                                        F("the file is larger than 4 GiB")});
    }
    char tag[mado::kFileEtagSize] = {};
    const uint32_t size = length.bytes;
    const bool tagged = mado::file_etag(&path[0], mado::FileStamp{size, state().version}, tag);
    char wanted[2] = {};
    const bool download =
        exchange.query_param(F("download"), &wanted[0], sizeof(wanted)) && (wanted[0] == '1');
    const mado::FileReply reply = {size,
                                   mado::content_type_for(&path[1]),
                                   false,
                                   tagged ? &tag[0] : nullptr,
                                   download ? &path[1] : nullptr,
                                   nullptr};
    const bool sent = mado::send_file(exchange, file, reply);
    file.close();
    return sent;
  }

  static mado::Problem receive(Exchange& exchange, uint8_t (&digest)[mado::kSha256Size]) {
    char temporary[kFilesPathSize] = {};
    static_cast<void>(strncpy_P(&temporary[0], &kFilesTemporary[0], sizeof(temporary) - 1U));
    static_cast<void>(state().sd->remove(&temporary[0]));
    FileT out = state().sd->open(&temporary[0], kWriteMode);
    if (!out) {
      return mado::Problem{mado::http::kInsufficientStorage, F("the card refused the write")};
    }
    mado::Sha256 sha;
    uint8_t chunk[kFilesChunkSize] = {};
    bool stored = true;
    for (uint32_t i = 0U; stored && (exchange.remaining_body() > 0U) && (i < kFilesMaxReads); ++i) {
      const int16_t got = exchange.read_body(&chunk[0], sizeof(chunk));
      const size_t count = (got > 0) ? static_cast<size_t>(got) : 0U;
      stored = (got > 0) && (out.write(&chunk[0], count) == count) && sha.update(&chunk[0], count);
    }
    out.close();
    const bool complete = stored && (exchange.remaining_body() == 0U) && sha.finish(&digest[0]);
    return complete
               ? mado::Problem{mado::http::kNoResponse, nullptr}
               : mado::Problem{stored ? mado::http::kBadRequest : mado::http::kInsufficientStorage,
                               F("the body ended early or the card refused the write")};
  }

  static void remove_temporary() {
    char temporary[kFilesPathSize] = {};
    static_cast<void>(strncpy_P(&temporary[0], &kFilesTemporary[0], sizeof(temporary) - 1U));
    const bool removed = state().sd->remove(&temporary[0]);
    static_cast<void>(removed);
  }

  static bool install(const char* const path) {
    char temporary[kFilesPathSize] = {};
    static_cast<void>(strncpy_P(&temporary[0], &kFilesTemporary[0], sizeof(temporary) - 1U));
    const bool existed = state().sd->exists(path);
    const bool cleared = (!existed) || state().sd->remove(path);
    FileT from = state().sd->open(&temporary[0], kReadMode);
    FileT to = state().sd->open(path, kWriteMode);
    const bool opened = cleared && static_cast<bool>(from) && static_cast<bool>(to);
    const mado::FileLength length = opened ? mado::file_length(from) : mado::FileLength{false, 0U};
    const bool copied = length.fits && mado::copy_file(from, to, length.bytes);
    from.close();
    to.close();
    static_cast<void>(state().sd->remove(&temporary[0]));
    changed(existed || copied);
    return copied;
  }

  static bool put_file(Exchange& exchange) {
    char path[kFilesPathSize] = {};
    if (!file_path(exchange, path)) {
      return bad_name(exchange);
    }
    if (exchange.content_length() > kFilesMaxUpload) {
      return mado::send_problem(
          exchange, mado::Problem{mado::http::kContentTooLarge, F("files are at most 8 MiB")});
    }
    uint8_t expected[mado::kSha256Size] = {};
    const bool checked = mado::parse_repr_digest(exchange.header(F("Repr-Digest")), expected);
    uint8_t digest[mado::kSha256Size] = {};
    const mado::Problem problem = receive(exchange, digest);
    if (problem.status != mado::http::kNoResponse) {
      remove_temporary();
      return mado::send_problem(exchange, problem);
    }
    if (checked && (!mado::constant_time_equal(&expected[0], &digest[0], sizeof(digest)))) {
      remove_temporary();
      return mado::send_problem(exchange, mado::Problem{mado::http::kUnprocessableContent,
                                                        F("the body does not match Repr-Digest")});
    }
    return finish_put(exchange, path);
  }

  static bool finish_put(Exchange& exchange, const char (&path)[kFilesPathSize]) {
    const bool existed = state().sd->exists(&path[0]);
    if (!install(&path[0])) {
      return mado::send_problem(exchange, mado::Problem{mado::http::kInsufficientStorage,
                                                        F("the card refused the write")});
    }
    if (existed) {
      return exchange.send_empty(mado::http::kNoContent);
    }
    char location[kFilesLocationSize] = {};
    const char* const prefix = mado::flash_address(rest_files::get_file_url());
    static_cast<void>(strncpy_P(&location[0], prefix, sizeof(location) - 1U));
    const bool encoded = encode_name(&path[1], location);
    if (!encoded) {
      return exchange.send_empty(mado::http::kInternalServerError);
    }
    mado::Reply<Exchange> reply(exchange);
    return reply.status(mado::http::kCreated).header(F("Location"), &location[0]).length(0U).end();
  }

  static bool unreserved(const char symbol) {
    const bool alpha = ((symbol >= 'a') && (symbol <= 'z')) || ((symbol >= 'A') && (symbol <= 'Z'));
    const bool digit = (symbol >= '0') && (symbol <= '9');
    return alpha || digit || (symbol == '-') || (symbol == '.') || (symbol == '_') ||
           (symbol == '~');
  }

  static char hex_digit(const uint8_t nibble) {
    const bool valid = mado::check(nibble < 16U);
    const uint8_t base =
        (nibble < 10U) ? static_cast<uint8_t>('0') : static_cast<uint8_t>('A' - 10);
    return valid ? static_cast<char>(base + nibble) : '0';
  }

  static bool encode_name(const char* const name, char (&location)[kFilesLocationSize]) {
    const bool valid = mado::check(name != nullptr) && mado::check(location[0] == '/');
    size_t used = valid ? strnlen(&location[0], kFilesLocationSize) : kFilesLocationSize;
    for (uint8_t i = 0U; valid && (i < mado::kCardNameSize) && (name[i] != '\0'); ++i) {
      const uint8_t code = static_cast<uint8_t>(name[i]);
      const bool plain = unreserved(name[i]);
      const size_t width = plain ? 1U : 3U;
      if ((used + width) >= kFilesLocationSize) {
        return false;
      }
      if (plain) {
        location[used] = name[i];
      } else {
        location[used] = '%';
        location[used + 1U] = hex_digit(static_cast<uint8_t>(code >> 4U));
        location[used + 2U] = hex_digit(static_cast<uint8_t>(code & 0x0FU));
      }
      used += width;
    }
    return valid && mado::check(used < kFilesLocationSize);
  }

  static bool delete_file(Exchange& exchange) {
    char path[kFilesPathSize] = {};
    if (!file_path(exchange, path)) {
      return bad_name(exchange);
    }
    if (!state().sd->exists(&path[0])) {
      return missing(exchange);
    }
    const bool removed = state().sd->remove(&path[0]);
    changed(removed);
    return removed ? exchange.send_empty(mado::http::kNoContent)
                   : mado::send_problem(exchange, mado::Problem{mado::http::kInsufficientStorage,
                                                                F("the card refused the delete")});
  }
};

#endif
