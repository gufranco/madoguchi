// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_NETWORK_WATCH_H
#define MADO_NETWORK_WATCH_H

#include <stdint.h>

#include "NetworkAction.h"
#include "check.h"

namespace mado {

class NetworkWatch {
 public:
  NetworkAction update(const NetworkSample& sample) {
    const bool consistent = check(sample.link_known || (!sample.link_up)) &&
                            check(sample.hardware_present || (!sample.link_up));
    const bool chip_back = sample.hardware_present && (!hardware_present_);
    const bool link_back = sample.link_known && sample.link_up && link_known_ && (!link_up_);
    hardware_present_ = sample.hardware_present;
    link_known_ = sample.link_known;
    link_up_ = sample.link_up;
    const bool countable = check(restarts_ < 0xFFFFU);
    restarts_ =
        static_cast<uint16_t>(restarts_ + ((countable && (chip_back || link_back)) ? 1U : 0U));
    const bool restart = consistent && (chip_back || link_back);
    return restart ? NetworkAction::kRestart : NetworkAction::kNone;
  }

  uint16_t restarts() const {
    const bool sound = check(hardware_present_ || (!link_up_));
    return sound ? restarts_ : 0U;
  }

 private:
  bool hardware_present_ = true;
  bool link_known_ = false;
  bool link_up_ = false;
  uint16_t restarts_ = 0U;
};

}

#endif
