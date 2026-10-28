// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef FORMAT_APP_H
#define FORMAT_APP_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/FlashText.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/check.h>
#include <mado/flash_address.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>
#include <string.h>

#include "FormatJob.h"
#include "generated_api.h"

constexpr uint32_t kFormatMaxBody = 64U;
constexpr uint32_t kSectorsPerMib = 2048UL;
constexpr uint8_t kSdhcType = 3U;

template <typename DriverT>
struct FormatState {
  typename DriverT::Device* device = nullptr;
  FormatJob<DriverT> job;
};

template <typename ClientT, typename DriverT>
struct FormatApp {
  using Exchange = mado::Exchange<ClientT, rest_format::Limits>;
  using Device = typename DriverT::Device;

  static FormatState<DriverT>& state() {
    static FormatState<DriverT> instance = {};
    return instance;
  }

  static void begin() {
    state() = FormatState<DriverT>{};
    state().device = DriverT::open();
  }

  static void step() {
    state().job.step();
  }

  static bool busy() {
    return state().job.busy();
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    const bool known = mado::check(route <= rest_format::kStartFormat);
    if (!known) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    switch (route) {
      case rest_format::kGetCard: return get_card(exchange);
      case rest_format::kGetFormat: return get_format(exchange);
      default: return start_format(exchange);
    }
  }

 private:
  static Device* ready_device() {
    Device* device = state().device;
    const bool answering = (device != nullptr) && (device->errorCode() == 0U);
    if ((!answering) && (!busy())) {
      device = DriverT::open();
      state().device = device;
    }
    return ((device != nullptr) && (device->errorCode() == 0U)) ? device : nullptr;
  }

  static bool unavailable(Exchange& exchange) {
    return mado::send_problem(
        exchange, mado::Problem{mado::http::kServiceUnavailable, F("no SD card answers")});
  }

  static void copy_text(char* const out, const size_t size, const __FlashStringHelper* text) {
    const bool valid = mado::check(out != nullptr) && mado::check(size > 0U);
    if (valid) {
      static_cast<void>(strncpy_P(out, mado::flash_address(text), size - 1U));
    }
  }

  static const __FlashStringHelper* file_system_name(const CardFileSystem kind) {
    switch (kind) {
      case CardFileSystem::kFat16: return F("FAT16");
      case CardFileSystem::kFat32: return F("FAT32");
      case CardFileSystem::kExFat: return F("exFAT");
      default: return F("");
    }
  }

  static const __FlashStringHelper* kind_name(const uint8_t type, const uint32_t sectors) {
    const bool high_capacity = mado::check(type <= kSdhcType) && (type == kSdhcType);
    if (!high_capacity) {
      return F("SDSC");
    }
    return (sectors > kFat32SectorLimit) ? F("SDXC") : F("SDHC");
  }

  static const __FlashStringHelper* phase_name(const FormatProgress& progress) {
    if (progress.failed) {
      return F("failed");
    }
    switch (progress.phase) {
      case FormatPhase::kFillZero: return F("fill-zero");
      case FormatPhase::kCheckZero: return F("check-zero");
      case FormatPhase::kFillOne: return F("fill-one");
      case FormatPhase::kCheckOne: return F("check-one");
      case FormatPhase::kFormat: return F("format");
      case FormatPhase::kMount: return F("mount");
      case FormatPhase::kDone: return F("done");
      default: return F("idle");
    }
  }

  static int32_t capacity_of(const uint32_t sectors) {
    return static_cast<int32_t>(sectors / kSectorsPerMib);
  }

  static bool get_card(Exchange& exchange) {
    Device* const device = ready_device();
    if (device == nullptr) {
      return unavailable(exchange);
    }
    const uint32_t sectors = device->sectorCount();
    rest_format::Card card = {};
    card.capacity_mib = capacity_of(sectors);
    copy_text(&card.kind[0], sizeof(card.kind), kind_name(device->type(), sectors));
    copy_text(&card.file_system[0], sizeof(card.file_system),
              file_system_name(file_system_for(sectors)));
    mado::Reply<Exchange> reply(exchange);
    const bool head = reply.status(mado::http::kOk).json().no_store().end();
    return head && rest_format::write_card(exchange, card);
  }

  static rest_format::FormatStatus status_of(Device* const device) {
    const FormatProgress progress = state().job.progress();
    const bool started = progress.phase != FormatPhase::kIdle;
    const uint32_t sectors =
        started ? progress.sectors : ((device == nullptr) ? 0U : device->sectorCount());
    rest_format::FormatStatus status = {};
    copy_text(&status.phase[0], sizeof(status.phase), phase_name(progress));
    status.percent = static_cast<int32_t>(progress.percent);
    status.capacity_mib = capacity_of(sectors);
    copy_text(&status.file_system[0], sizeof(status.file_system),
              file_system_name(file_system_for(sectors)));
    copy_text(&status.detail[0], sizeof(status.detail),
              (progress.detail == nullptr) ? F("") : progress.detail);
    return status;
  }

  static bool get_format(Exchange& exchange) {
    const rest_format::FormatStatus status = status_of(busy() ? nullptr : ready_device());
    mado::Reply<Exchange> reply(exchange);
    const bool head = reply.status(mado::http::kOk).json().no_store().end();
    return head && rest_format::write_format_status(exchange, status);
  }

  static mado::Problem refusal(Device* const device, const rest_format::FormatRequest& request) {
    if (device == nullptr) {
      return mado::Problem{mado::http::kServiceUnavailable, F("no SD card answers")};
    }
    if (busy()) {
      return mado::Problem{mado::http::kConflict, F("a wipe is already running")};
    }
    if (request.capacity_mib != capacity_of(device->sectorCount())) {
      return mado::Problem{mado::http::kUnprocessableContent,
                           F("the capacity does not match this card")};
    }
    return mado::Problem{mado::http::kNoResponse, nullptr};
  }

  static bool start_format(Exchange& exchange) {
    rest_format::FormatRequest request = {};
    uint8_t present = 0U;
    const mado::Problem body =
        rest_format::read_format_request_body(exchange, request, present, kFormatMaxBody);
    Device* const device = busy() ? state().device : ready_device();
    const mado::Problem refused =
        (body.status != mado::http::kNoResponse) ? body : refusal(device, request);
    if (refused.status != mado::http::kNoResponse) {
      return mado::send_problem(exchange, refused);
    }
    if (!state().job.start(*device)) {
      return mado::send_problem(exchange, mado::Problem{mado::http::kUnprocessableContent,
                                                        F("the card reports no sectors")});
    }
    mado::Reply<Exchange> reply(exchange);
    const bool head = reply.status(mado::http::kAccepted)
                          .json()
                          .no_store()
                          .header(F("Location"), mado::FlashText(F("/api/card/format")))
                          .end();
    return head && rest_format::write_format_status(exchange, status_of(device));
  }
};

#endif
