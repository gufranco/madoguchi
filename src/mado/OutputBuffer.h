// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_OUTPUT_BUFFER_H
#define MADO_OUTPUT_BUFFER_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "GuardedBuffer.h"
#include "check.h"
#include "kMaxStringScan.h"

namespace mado {

constexpr uint16_t kMaxOutputBufferSize = 1460U;

template <typename ClientT, typename LimitsT>
class OutputBuffer {
 public:
  static_assert(LimitsT::kOutputBufferSize <= kMaxOutputBufferSize,
                "the output buffer holds at most one Ethernet segment");

  void attach(ClientT& client) {
    const bool barrier_ok = check(storage_.intact());
    const bool bounded = check(fill_ <= LimitsT::kOutputBufferSize);
    client_ = &client;
    fill_ = 0U;
    discard_ = false;
    failed_ = !(barrier_ok && bounded);
  }

  void detach() {
    const bool drained = check(fill_ == 0U);
    const bool barrier_ok = check(storage_.intact());
    if (!(drained && barrier_ok)) {
      fill_ = 0U;
      failed_ = true;
    }
    client_ = nullptr;
  }

  void set_discard(const bool discard) {
    const bool attached = check(client_ != nullptr);
    const bool bounded = check(fill_ <= LimitsT::kOutputBufferSize);
    if (!(attached && bounded)) {
      failed_ = true;
      return;
    }
    discard_ = discard;
  }

  size_t write_byte(const uint8_t value) {
    const bool attached = check(client_ != nullptr);
    if ((!attached) || failed_) {
      return 0U;
    }
    if (discard_) {
      return 1U;
    }
    const bool full = fill_ >= LimitsT::kOutputBufferSize;
    if (full) {
      const bool flushed = flush();
      if (!flushed) {
        return 0U;
      }
    }
    const bool in_bounds = check(fill_ < LimitsT::kOutputBufferSize);
    if (!in_bounds) {
      return 0U;
    }
    storage_.data()[fill_] = value;
    ++fill_;
    return 1U;
  }

  size_t write(const uint8_t* const data, const size_t length) {
    const bool valid = check(data != nullptr) || check(length == 0U);
    if (!valid) {
      return 0U;
    }
    const bool large = (length >= LimitsT::kOutputBufferSize) && (!discard_);
    if (large) {
      return write_direct(data, length);
    }
    size_t written = 0U;
    for (size_t i = 0U; i < length; ++i) {
      const bool in_step = check(written == i);
      if (!in_step) {
        break;
      }
      const size_t accepted = write_byte(data[i]);
      if (accepted == 0U) {
        break;
      }
      written += accepted;
    }
    return written;
  }

  size_t write_flash(const char* const flash) {
    const bool valid = check(flash != nullptr);
    if (!valid) {
      return 0U;
    }
    const size_t length = strlen_P(flash);
    const bool scannable = check(length < kMaxStringScan);
    if (!scannable) {
      return 0U;
    }
    size_t written = 0U;
    for (size_t i = 0U; i < length; ++i) {
      written += write_byte(pgm_read_byte(flash + i));
    }
    const bool bounded = check(written <= length);
    return bounded ? written : 0U;
  }

  bool flush() {
    const bool attached = check(client_ != nullptr);
    if (!attached) {
      return false;
    }
    const bool bounded = check(fill_ <= LimitsT::kOutputBufferSize);
    if ((fill_ == 0U) || (!bounded)) {
      return (!failed_) && bounded;
    }
    const uint16_t pending = fill_;
    const size_t sent = client_->write(storage_.data(), pending);
    fill_ = 0U;
    const bool complete = sent == pending;
    failed_ = failed_ || (!complete);
    return complete;
  }

  bool intact() const {
    const bool barrier_ok = check(storage_.intact());
    const bool bounded = check(fill_ <= LimitsT::kOutputBufferSize);
    return barrier_ok && bounded;
  }

 private:
  size_t write_direct(const uint8_t* const data, const size_t length) {
    const bool attached = check(client_ != nullptr) && check(length > 0U);
    const bool flushed = attached && (!failed_) && flush();
    if (!flushed) {
      return 0U;
    }
    const size_t sent = client_->write(data, length);
    const bool complete = sent == length;
    failed_ = failed_ || (!complete);
    return complete ? length : 0U;
  }

  GuardedBuffer<uint8_t, LimitsT::kOutputBufferSize> storage_;
  ClientT* client_ = nullptr;
  uint16_t fill_ = 0U;
  bool discard_ = false;
  bool failed_ = false;
};

}

#endif
