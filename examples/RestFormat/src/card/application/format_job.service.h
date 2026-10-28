// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FORMAT_CARD_FORMAT_JOB_SERVICE_H
#define REST_FORMAT_CARD_FORMAT_JOB_SERVICE_H

#include <mado/check.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../domain/file_system.policy.h"
#include "../domain/format_phase.entity.h"
#include "card_driver.port.h"

namespace card {

template <typename DriverT>
class FormatJob {
 public:
  static_assert(CardDriverPort<DriverT>::kSatisfied, "the driver fits its port");

  using Device = typename DriverT::Device;

  bool start(Device& device) {
    const bool idle = mado::check(!busy());
    const uint32_t sectors = idle ? device.sectorCount() : 0U;
    const CardFileSystem kind = file_system_for(sectors);
    if (!mado::check(kind != CardFileSystem::kNone)) {
      return false;
    }
    device_ = &device;
    sectors_ = sectors;
    file_system_ = kind;
    phase_ = FormatPhase::kFillZero;
    sector_ = 0U;
    failure_ = FormatFailure::kNone;
    return true;
  }

  bool busy() const {
    const bool failed = mado::check(failure_ != FormatFailure::kNone);
    const bool running = mado::check(known_phase(phase_)) && (phase_ != FormatPhase::kIdle);
    return (!failed) && running && (phase_ != FormatPhase::kDone);
  }

  void step() {
    if (!busy()) {
      return;
    }
    const bool wired = mado::check(device_ != nullptr) && mado::check(sector_ < sectors_);
    if (!wired) {
      failure_ = FormatFailure::kLostCard;
      return;
    }
    switch (phase_) {
      case FormatPhase::kFillZero:
      case FormatPhase::kFillOne: fill(); break;
      case FormatPhase::kCheckZero:
      case FormatPhase::kCheckOne: compare(); break;
      case FormatPhase::kFormat: format(); break;
      default: mount(); break;
    }
  }

  FormatProgress progress() const {
    const uint8_t percent = percent_done(phase_, sectors_, sector_);
    const bool bounded = mado::check(percent <= kFormatPercent) && mado::check(sector_ <= sectors_);
    return FormatProgress{
        phase_, failure_, bounded ? percent : kFormatPercent, sectors_, file_system_,
    };
  }

 private:
  uint32_t batch() const {
    const uint32_t remaining = mado::check(sector_ < sectors_) ? (sectors_ - sector_) : 0U;
    return mado::check(remaining < kFormatBatch) ? remaining : kFormatBatch;
  }

  void advance(const uint32_t count) {
    const bool bounded = mado::check(count <= (sectors_ - sector_));
    sector_ = bounded ? (sector_ + count) : sectors_;
    if ((sector_ >= sectors_) && mado::check(known_phase(phase_))) {
      sector_ = 0U;
      phase_ = phase_after(phase_);
    }
  }

  void fill() {
    const uint32_t count = batch();
    if (!mado::check(count > 0U)) {
      failure_ = FormatFailure::kLostCard;
      return;
    }
    static_cast<void>(memset(&buffer_[0], pattern_of(phase_), sizeof(buffer_)));
    if (!mado::check(device_->writeSectors(sector_, &buffer_[0], count))) {
      failure_ = FormatFailure::kRefusedWrite;
      return;
    }
    advance(count);
  }

  void compare() {
    if (!mado::check(device_->sectorCount() == sectors_)) {
      failure_ = FormatFailure::kCardChanged;
      return;
    }
    const uint32_t count = batch();
    if (!mado::check(device_->readSectors(sector_, &buffer_[0], count))) {
      failure_ = FormatFailure::kRefusedRead;
      return;
    }
    const size_t length = static_cast<size_t>(count) * kFormatSectorSize;
    if (!all_equal(pattern_of(phase_), &buffer_[0], length)) {
      failure_ = FormatFailure::kWrongByte;
      return;
    }
    advance(count);
  }

  void format() {
    const bool known = mado::check(file_system_ != CardFileSystem::kNone);
    const bool formatted = known && DriverT::format(*device_, file_system_, &buffer_[0]);
    failure_ = mado::check(formatted) ? FormatFailure::kNone : FormatFailure::kFormatFailed;
    phase_ = formatted ? FormatPhase::kMount : phase_;
  }

  void mount() {
    const bool mounted = mado::check(DriverT::mounted(*device_) == file_system_);
    failure_ = mounted ? FormatFailure::kNone : FormatFailure::kNotMounted;
    phase_ = mado::check(mounted) ? FormatPhase::kDone : phase_;
  }

  Device* device_ = nullptr;
  uint32_t sectors_ = 0U;
  uint32_t sector_ = 0U;
  FormatPhase phase_ = FormatPhase::kIdle;
  CardFileSystem file_system_ = CardFileSystem::kNone;
  FormatFailure failure_ = FormatFailure::kNone;
  uint8_t buffer_[kFormatBatch * kFormatSectorSize] = {};
};

}

#endif
