// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FORMAT_CARD_FORMAT_CONTROLLER_H
#define REST_FORMAT_CARD_FORMAT_CONTROLLER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/FlashText.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/name_at.h>
#include <mado/send_problem.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../http/api.generated.h"
#include "../application/format.service.h"
#include "../domain/file_system.policy.h"
#include "../domain/format_phase.entity.h"

namespace card {

constexpr uint32_t kFormatMaxBody = 64U;
constexpr uint8_t kKindCount = 3U;
constexpr uint8_t kFileSystemCount = 4U;

const char kNameSdsc[] PROGMEM = "SDSC";
const char kNameSdhc[] PROGMEM = "SDHC";
const char kNameSdxc[] PROGMEM = "SDXC";
const char* const kKindNames[kKindCount] PROGMEM = {&kNameSdsc[0], &kNameSdhc[0], &kNameSdxc[0]};

const char kNameNone[] PROGMEM = "";
const char kNameFat16[] PROGMEM = "FAT16";
const char kNameFat32[] PROGMEM = "FAT32";
const char kNameExFat[] PROGMEM = "exFAT";
const char* const kFileSystemNames[kFileSystemCount] PROGMEM = {
    &kNameNone[0],
    &kNameFat16[0],
    &kNameFat32[0],
    &kNameExFat[0],
};

const char kPhaseIdle[] PROGMEM = "idle";
const char kPhaseFillZero[] PROGMEM = "fill-zero";
const char kPhaseCheckZero[] PROGMEM = "check-zero";
const char kPhaseFillOne[] PROGMEM = "fill-one";
const char kPhaseCheckOne[] PROGMEM = "check-one";
const char kPhaseFormat[] PROGMEM = "format";
const char kPhaseMount[] PROGMEM = "mount";
const char kPhaseDone[] PROGMEM = "done";
const char kPhaseFailed[] PROGMEM = "failed";
const char* const kPhaseNames[kPhaseCount + 1U] PROGMEM = {
    &kPhaseIdle[0],    &kPhaseFillZero[0], &kPhaseCheckZero[0],
    &kPhaseFillOne[0], &kPhaseCheckOne[0], &kPhaseFormat[0],
    &kPhaseMount[0],   &kPhaseDone[0],     &kPhaseFailed[0],
};

const char kLostCard[] PROGMEM = "the job lost its card";
const char kRefusedWrite[] PROGMEM = "the card refused a write";
const char kCardChanged[] PROGMEM = "the card changed during the wipe";
const char kRefusedRead[] PROGMEM = "the card refused a read";
const char kWrongByte[] PROGMEM = "the card read back a byte it was not given";
const char kFormatFailed[] PROGMEM = "the format did not complete";
const char kNotMounted[] PROGMEM = "the new volume did not mount";
const char* const kFailureDetails[kFailureCount] PROGMEM = {
    &kNameNone[0],    &kLostCard[0],  &kRefusedWrite[0], &kCardChanged[0],
    &kRefusedRead[0], &kWrongByte[0], &kFormatFailed[0], &kNotMounted[0],
};

const char kFreshFat16[] PROGMEM = "the card holds a fresh FAT16 volume";
const char kFreshFat32[] PROGMEM = "the card holds a fresh FAT32 volume";
const char kFreshExFat[] PROGMEM = "the card holds a fresh exFAT volume";
const char* const kDoneDetails[kFileSystemCount] PROGMEM = {
    &kNameNone[0],
    &kFreshFat16[0],
    &kFreshFat32[0],
    &kFreshExFat[0],
};

template <typename ExchangeT, typename ServiceT>
class FormatController {
 public:
  static bool get_card(ExchangeT& exchange, ServiceT& service) {
    CardFacts facts = {};
    if (!mado::check(service.describe(facts))) {
      return mado::send_problem(exchange, refusal(StartStatus::kNoCard));
    }
    rest_format::Card dto = {};
    dto.capacity_mib = facts.capacity_mib;
    copy_name(&dto.kind[0], sizeof(dto.kind), &kKindNames[0], kKindCount,
              static_cast<uint8_t>(facts.kind));
    copy_name(&dto.file_system[0], sizeof(dto.file_system), &kFileSystemNames[0], kFileSystemCount,
              static_cast<uint8_t>(facts.file_system));
    mado::Reply<ExchangeT> reply(exchange);
    const bool head =
        mado::check(dto.kind[0] != '\0') && reply.status(mado::http::kOk).json().no_store().end();
    return head && rest_format::write_card(exchange, dto);
  }

  static bool get_format(ExchangeT& exchange, ServiceT& service) {
    const rest_format::FormatStatus dto = to_dto(service.status());
    mado::Reply<ExchangeT> reply(exchange);
    const bool named = mado::check(dto.phase[0] != '\0') && mado::check(dto.percent <= 100);
    const bool head = named && reply.status(mado::http::kOk).json().no_store().end();
    return head && rest_format::write_format_status(exchange, dto);
  }

  static bool start(ExchangeT& exchange, ServiceT& service) {
    rest_format::FormatRequest request = {};
    uint8_t present = 0U;
    const mado::Problem body =
        rest_format::read_format_request_body(exchange, request, present, kFormatMaxBody);
    if (!mado::check(body.status == mado::http::kNoResponse)) {
      return mado::send_problem(exchange, body);
    }
    const StartStatus status = service.start(request.capacity_mib);
    if (!mado::check(status == StartStatus::kAccepted)) {
      return mado::send_problem(exchange, refusal(status));
    }
    mado::Reply<ExchangeT> reply(exchange);
    const bool head = reply.status(mado::http::kAccepted)
                          .json()
                          .no_store()
                          .header(F("Location"), mado::FlashText(F("/api/card/format")))
                          .end();
    return head && rest_format::write_format_status(exchange, to_dto(service.status()));
  }

 private:
  static mado::Problem refusal(const StartStatus status) {
    const bool known =
        mado::check(static_cast<uint8_t>(status) <= static_cast<uint8_t>(StartStatus::kNoSectors));
    if (!(known && mado::check(status != StartStatus::kAccepted))) {
      return mado::Problem{mado::http::kInternalServerError, nullptr};
    }
    switch (status) {
      case StartStatus::kNoCard:
        return mado::Problem{mado::http::kServiceUnavailable, F("no SD card answers")};
      case StartStatus::kBusy:
        return mado::Problem{mado::http::kConflict, F("a wipe is already running")};
      case StartStatus::kWrongCapacity:
        return mado::Problem{
            mado::http::kUnprocessableContent,
            F("the capacity does not match this card"),
        };
      default:
        return mado::Problem{mado::http::kUnprocessableContent, F("the card reports no sectors")};
    }
  }

  static void copy_name(char* const out, const size_t size, const char* const* const table,
                        const uint8_t count, const uint8_t index) {
    const bool valid = mado::check(out != nullptr) && mado::check(size > 0U);
    const char* const name = mado::check(index < count) ? mado::name_at(table, index) : nullptr;
    if (valid) {
      out[0] = '\0';
    }
    if (valid && mado::check(name != nullptr)) {
      static_cast<void>(strncpy_P(out, name, size - 1U));
    }
  }

  static rest_format::FormatStatus to_dto(const FormatProgress& progress) {
    const bool failed = mado::check(progress.failure != FormatFailure::kNone);
    const bool done = mado::check(progress.phase == FormatPhase::kDone) && (!failed);
    const uint8_t phase = failed ? kPhaseCount : static_cast<uint8_t>(progress.phase);
    const uint8_t system = static_cast<uint8_t>(progress.file_system);
    rest_format::FormatStatus dto = {};
    copy_name(&dto.phase[0], sizeof(dto.phase), &kPhaseNames[0], kPhaseCount + 1U, phase);
    dto.percent = static_cast<int32_t>(progress.percent);
    dto.capacity_mib = capacity_mib(progress.sectors);
    copy_name(&dto.file_system[0], sizeof(dto.file_system), &kFileSystemNames[0], kFileSystemCount,
              system);
    if (done) {
      copy_name(&dto.detail[0], sizeof(dto.detail), &kDoneDetails[0], kFileSystemCount, system);
    } else {
      copy_name(&dto.detail[0], sizeof(dto.detail), &kFailureDetails[0], kFailureCount,
                static_cast<uint8_t>(progress.failure));
    }
    return dto;
  }
};

}

#endif
