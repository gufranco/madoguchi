// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FORMAT_CARD_FORMAT_SERVICE_H
#define REST_FORMAT_CARD_FORMAT_SERVICE_H

#include <mado/check.h>
#include <stdint.h>

#include "../domain/file_system.policy.h"
#include "../domain/format_phase.entity.h"
#include "format_job.service.h"

namespace card {

struct CardFacts {
  int32_t capacity_mib;
  CardKind kind;
  CardFileSystem file_system;
};

enum class StartStatus : uint8_t { kAccepted, kNoCard, kBusy, kWrongCapacity, kNoSectors };

template <typename DriverT>
class FormatService {
 public:
  using Device = typename DriverT::Device;

  bool begin() {
    job_ = FormatJob<DriverT>{};
    device_ = DriverT::open();
    return mado::check(device_ != nullptr);
  }

  void step() {
    if (mado::check(job_.busy())) {
      job_.step();
    }
  }

  bool describe(CardFacts& facts) {
    Device* const device = answering();
    if (!mado::check(device != nullptr)) {
      return false;
    }
    const uint32_t sectors = device->sectorCount();
    facts = CardFacts{
        capacity_mib(sectors),
        kind_for(CardGeometry{device->type(), sectors}),
        file_system_for(sectors),
    };
    return mado::check(facts.capacity_mib >= 0);
  }

  FormatProgress status() {
    FormatProgress progress = job_.progress();
    const bool started = mado::check(progress.phase != FormatPhase::kIdle);
    Device* const device = (started || job_.busy()) ? nullptr : answering();
    progress.sectors = (started || (device == nullptr)) ? progress.sectors : device->sectorCount();
    progress.file_system = file_system_for(progress.sectors);
    return mado::check(progress.percent <= kFormatPercent) ? progress : job_.progress();
  }

  StartStatus start(const int32_t requested_mib) {
    Device* const device = job_.busy() ? device_ : answering();
    if (!mado::check(device != nullptr)) {
      return StartStatus::kNoCard;
    }
    if (!mado::check(!job_.busy())) {
      return StartStatus::kBusy;
    }
    if (!mado::check(requested_mib == capacity_mib(device->sectorCount()))) {
      return StartStatus::kWrongCapacity;
    }
    return job_.start(*device) ? StartStatus::kAccepted : StartStatus::kNoSectors;
  }

 private:
  Device* answering() {
    const bool answers = (device_ != nullptr) && mado::check(device_->errorCode() == 0U);
    if ((!answers) && (!job_.busy())) {
      device_ = DriverT::open();
    }
    return ((device_ != nullptr) && mado::check(device_->errorCode() == 0U)) ? device_ : nullptr;
  }

  Device* device_ = nullptr;
  FormatJob<DriverT> job_;
};

}

#endif
