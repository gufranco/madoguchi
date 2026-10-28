// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FILES_FILES_FILES_CONTROLLER_H
#define REST_FILES_FILES_FILES_CONTROLLER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/DecodeMode.h>
#include <mado/FileReply.h>
#include <mado/FileStamp.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/check.h>
#include <mado/content_type_for.h>
#include <mado/file_etag.h>
#include <mado/flash_address.h>
#include <mado/http_status.h>
#include <mado/parse_repr_digest.h>
#include <mado/send_file.h>
#include <mado/send_problem.h>
#include <mado/url_decode.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../http/api.generated.h"
#include "../application/file_library.service.h"
#include "../domain/file_path.value.h"
#include "../domain/upload.policy.h"
#include "percent.encoding.h"

namespace files {

constexpr int32_t kMaxListedSize = 0x7FFFFFFFL;

template <typename ExchangeT, typename LibraryT, typename FileT>
class FilesController {
 public:
  static bool list(ExchangeT& exchange, LibraryT& library) {
    ListWriter writer(exchange);
    const ListStatus status = library.list(writer);
    if (!mado::check(status != ListStatus::kNoRoot)) {
      return mado::send_problem(exchange, mado::Problem{
                                              mado::http::kServiceUnavailable,
                                              F("the SD card has no root folder"),
                                          });
    }
    return mado::check(status == ListStatus::kListed) && (exchange.print(F("]")) > 0U);
  }

  static bool get(ExchangeT& exchange, LibraryT& library) {
    FilePath path = {};
    if (!mado::check(path_of(exchange, path))) {
      return refuse(exchange, Refusal::kBadName);
    }
    FileT file;
    mado::FileStamp stamp = {};
    const OpenStatus status = library.open(path, file, stamp);
    if (!mado::check(status == OpenStatus::kOpened)) {
      file.close();
      return refuse(exchange,
                    (status == OpenStatus::kMissing) ? Refusal::kMissing : Refusal::kTooLarge);
    }
    char tag[mado::kFileEtagSize] = {};
    const bool tagged = mado::file_etag(&path.text[0], stamp, tag);
    char wanted[2] = {};
    const bool download =
        exchange.query_param(F("download"), &wanted[0], sizeof(wanted)) && (wanted[0] == '1');
    const mado::FileReply reply = {
        stamp.size,
        mado::content_type_for(&path.text[1]),
        false,
        tagged ? &tag[0] : nullptr,
        download ? &path.text[1] : nullptr,
        nullptr,
    };
    const bool sent = mado::send_file(exchange, file, reply);
    file.close();
    return sent;
  }

  static bool put(ExchangeT& exchange, LibraryT& library) {
    FilePath path = {};
    if (!mado::check(path_of(exchange, path))) {
      return refuse(exchange, Refusal::kBadName);
    }
    if (!upload_fits(exchange.content_length())) {
      return mado::send_problem(
          exchange, mado::Problem{mado::http::kContentTooLarge, F("files are at most 8 MiB")});
    }
    DigestClaim claim = {};
    claim.given = mado::parse_repr_digest(exchange.header(F("Repr-Digest")), claim.bytes);
    const StoreStatus status = library.store(path, exchange, claim);
    if (!mado::check((status == StoreStatus::kCreated) || (status == StoreStatus::kReplaced))) {
      return mado::send_problem(exchange, refusal(status));
    }
    return (status == StoreStatus::kReplaced) ? exchange.send_empty(mado::http::kNoContent)
                                              : send_created(exchange, path);
  }

  static bool remove(ExchangeT& exchange, LibraryT& library) {
    FilePath path = {};
    if (!mado::check(path_of(exchange, path))) {
      return refuse(exchange, Refusal::kBadName);
    }
    const RemoveStatus status = library.remove(path);
    if (!mado::check(status == RemoveStatus::kRemoved)) {
      return (status == RemoveStatus::kMissing)
                 ? refuse(exchange, Refusal::kMissing)
                 : mado::send_problem(exchange, mado::Problem{
                                                    mado::http::kInsufficientStorage,
                                                    F("the card refused the delete"),
                                                });
    }
    return exchange.send_empty(mado::http::kNoContent);
  }

 private:
  enum class Refusal : uint8_t { kBadName, kMissing, kTooLarge };

  class ListWriter {
   public:
    explicit ListWriter(ExchangeT& exchange)
        : exchange_(&exchange), listed_(mado::check(!exchange.status_sent()) ? 0U : kListMax) {
    }

    bool open() {
      const bool fresh = mado::check(listed_ == 0U);
      mado::Reply<ExchangeT> reply(*exchange_);
      return fresh && reply.status(mado::http::kOk).json().no_store().end() &&
             (exchange_->print(F("[")) > 0U);
    }

    bool visit(const FileEntry& entry) {
      const bool named = mado::check(entry.name[0] != '\0');
      const bool separated =
          mado::check(listed_ < kListMax) && ((listed_ == 0U) || (exchange_->print(F(",")) > 0U));
      listed_ = static_cast<uint8_t>(listed_ + 1U);
      return named && separated && rest_files::write_file_entry(*exchange_, to_dto(entry));
    }

   private:
    ExchangeT* exchange_;
    uint8_t listed_;
  };

  static rest_files::FileEntry to_dto(const FileEntry& entry) {
    static_assert(sizeof(rest_files::FileEntry::name) == mado::kCardNameSize, "the schema agrees");
    const uint64_t listed = static_cast<uint64_t>(kMaxListedSize);
    const bool fits = mado::check(entry.size <= listed);
    const bool named = mado::check(entry.name[mado::kCardNameSize - 1U] == '\0');
    rest_files::FileEntry dto = {};
    static_cast<void>(memcpy(&dto.name[0], &entry.name[0], named ? sizeof(dto.name) : 0U));
    dto.size = static_cast<int32_t>(fits ? entry.size : listed);
    dto.directory = entry.directory;
    return dto;
  }

  static bool path_of(const ExchangeT& exchange, FilePath& path) {
    const char* const prefix = mado::flash_address(rest_files::get_file_url());
    const size_t skip = strlen_P(prefix);
    const char* const full = mado::check(exchange.path() != nullptr) ? exchange.path() : "";
    const bool prefixed = mado::check(strlen(full) >= skip) && (strncmp_P(full, prefix, skip) == 0);
    const char* const name = prefixed ? &full[skip] : "";
    path.text[0] = '/';
    const int16_t length = mado::url_decode(name, static_cast<uint16_t>(strlen(name)),
                                            &path.text[1], kPathSize - 1U, mado::DecodeMode::kPath);
    return mado::check(length > 0) && is_file_path(path);
  }

  static mado::Problem refusal(const StoreStatus status) {
    const bool failed = mado::check(status != StoreStatus::kCreated);
    if (failed && (status == StoreStatus::kDigestMismatch)) {
      return mado::Problem{
          mado::http::kUnprocessableContent,
          F("the body does not match Repr-Digest"),
      };
    }
    if (mado::check(status == StoreStatus::kIncomplete)) {
      return mado::Problem{
          mado::http::kBadRequest,
          F("the body ended early or the card refused the write"),
      };
    }
    return mado::Problem{mado::http::kInsufficientStorage, F("the card refused the write")};
  }

  static bool send_created(ExchangeT& exchange, const FilePath& path) {
    char location[kLocationSize] = {};
    const char* const prefix = mado::flash_address(rest_files::get_file_url());
    static_cast<void>(strncpy_P(&location[0], prefix, sizeof(location) - 1U));
    if (!mado::check(encode_path(path, location))) {
      return exchange.send_empty(mado::http::kInternalServerError);
    }
    mado::Reply<ExchangeT> reply(exchange);
    return reply.status(mado::http::kCreated).header(F("Location"), &location[0]).length(0U).end();
  }

  static bool refuse(ExchangeT& exchange, const Refusal reason) {
    const bool fresh = mado::check(!exchange.status_sent());
    if (mado::check(reason == Refusal::kBadName)) {
      return fresh &&
             mado::send_problem(exchange, mado::Problem{
                                              mado::http::kBadRequest,
                                              F("file names are 1 to 64 printable ASCII "
                                                "characters without \\ / : * ? \" < > | %"),
                                          });
    }
    return fresh && mado::send_problem(exchange, (reason == Refusal::kMissing)
                                                     ? mado::Problem{mado::http::kNotFound,
                                                                     F("no file has this name"),}
                                                     : mado::Problem{
                                                           mado::http::kInternalServerError,
                                                           F("the file is larger than 4 GiB"),
                                                       });
  }
};

}

#endif
