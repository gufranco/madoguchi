// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SHA256_H
#define MADO_SHA256_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "check.h"
#include "kSha256BlockSize.h"
#include "kSha256Size.h"

namespace mado {

class Sha256 {
 public:
  static constexpr uint8_t kStateWords = 8U;
  static constexpr uint32_t kMaxMessageBytes = 0x1FFFFFFFU;

  Sha256() {
    reset();
  }

  void reset() {
    const uint32_t* const initial = initial_state();
    for (uint8_t i = 0U; i < kStateWords; ++i) {
      state_[i] = pgm_read_dword(&initial[i]);
    }
    used_ = 0U;
    length_ = 0U;
    finished_ = false;
  }

  bool resume(const uint32_t* const state, const uint32_t length) {
    const bool valid = check(state != nullptr) && check((length % kSha256BlockSize) == 0U);
    const bool bounded = check(length <= kMaxMessageBytes);
    if (!(valid && bounded)) {
      return false;
    }
    for (uint8_t i = 0U; i < kStateWords; ++i) {
      state_[i] = state[i];
    }
    used_ = 0U;
    length_ = length;
    finished_ = false;
    return true;
  }

  bool export_state(uint32_t* const state) const {
    const bool valid = check(state != nullptr);
    const bool aligned = check(used_ == 0U) && check(!finished_);
    if (!(valid && aligned)) {
      return false;
    }
    for (uint8_t i = 0U; i < kStateWords; ++i) {
      state[i] = state_[i];
    }
    return true;
  }

  bool update(const uint8_t value) {
    const bool open = check(!finished_);
    const bool bounded = check(used_ < kSha256BlockSize) && check(length_ < kMaxMessageBytes);
    if (!(open && bounded)) {
      return false;
    }
    put_byte(value);
    length_ += 1U;
    if (used_ == kSha256BlockSize) {
      compress();
      used_ = 0U;
    }
    return true;
  }

  bool update(const uint8_t* const data, const size_t length) {
    const bool valid = check(data != nullptr) || check(length == 0U);
    const bool fits = check(length <= kMaxMessageBytes);
    if (!(valid && fits)) {
      return false;
    }
    bool accepted = true;
    for (size_t i = 0U; i < length; ++i) {
      accepted = update(data[i]);
      if (!accepted) {
        break;
      }
    }
    return accepted;
  }

  bool finish(uint8_t* const digest) {
    const bool valid = check(digest != nullptr);
    const bool open = check(!finished_) && check(used_ < kSha256BlockSize);
    if (!(valid && open)) {
      return false;
    }
    const uint32_t bits = length_ << 3U;
    pad_message();
    words_[kHighLengthWord] = 0U;
    words_[kLowLengthWord] = bits;
    compress();
    write_digest(digest);
    finished_ = true;
    return true;
  }

 private:
  static constexpr uint8_t kBlockWords = 16U;
  static constexpr uint8_t kRounds = 64U;
  static constexpr uint8_t kLengthOffset = 56U;
  static constexpr uint8_t kHighLengthWord = 14U;
  static constexpr uint8_t kLowLengthWord = 15U;
  static constexpr uint8_t kWordBits = 32U;
  static constexpr uint8_t kBytesPerWord = 4U;
  static constexpr uint8_t kPadMarker = 0x80U;

  static const uint32_t* initial_state() {
    static const uint32_t kInitial[kStateWords] PROGMEM = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
    };
    return &kInitial[0];
  }

  static const uint32_t* round_constants() {
    static const uint32_t kRoundConstants[kRounds] PROGMEM = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
        0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
        0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
        0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
        0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
        0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
        0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
        0xc67178f2U,
    };
    return &kRoundConstants[0];
  }

  template <uint8_t Count>
  static uint32_t rotate_right(const uint32_t value) {
    static_assert(Count > 0U, "a rotation moves at least one bit");
    static_assert(Count < kWordBits, "a rotation moves less than a word");
    const uint32_t left = static_cast<uint32_t>(kWordBits) - static_cast<uint32_t>(Count);
    return (value >> Count) | (value << left);
  }

  void put_byte(const uint8_t value) {
    const bool bounded = check(used_ < kSha256BlockSize) && check(!finished_);
    if (bounded) {
      const uint8_t word = static_cast<uint8_t>(used_ / kBytesPerWord);
      const uint8_t shift = static_cast<uint8_t>(24U - ((used_ % kBytesPerWord) * 8U));
      const bool word_start = (used_ % kBytesPerWord) == 0U;
      const uint32_t kept = word_start ? 0U : words_[word];
      words_[word] = kept | (static_cast<uint32_t>(value) << shift);
      used_ = static_cast<uint8_t>(used_ + 1U);
    }
  }

  void zero_fill(const uint8_t limit) {
    const bool valid =
        check(limit <= kSha256BlockSize) && check(used_ <= limit) && check(!finished_);
    if (valid) {
      for (uint8_t i = used_; i < limit; ++i) {
        put_byte(0U);
      }
    }
  }

  void pad_message() {
    const bool open = check(used_ < kSha256BlockSize) && check(!finished_);
    if (open) {
      put_byte(kPadMarker);
      if (used_ > kLengthOffset) {
        zero_fill(kSha256BlockSize);
        compress();
        used_ = 0U;
      }
      zero_fill(kLengthOffset);
      const bool placed = check(used_ == kLengthOffset);
      used_ = placed ? used_ : kLengthOffset;
    }
  }

  uint32_t next_schedule(const uint8_t round) {
    const bool valid = check(round >= kBlockWords) && check(round < kRounds) && check(!finished_);
    const uint8_t slot = static_cast<uint8_t>(round % kBlockWords);
    const uint32_t early = words_[(round + 1U) % kBlockWords];
    const uint32_t late = words_[(round + 14U) % kBlockWords];
    const uint32_t sigma0 = rotate_right<7U>(early) ^ rotate_right<18U>(early) ^ (early >> 3U);
    const uint32_t sigma1 = rotate_right<17U>(late) ^ rotate_right<19U>(late) ^ (late >> 10U);
    const uint32_t next = words_[slot] + sigma0 + words_[(round + 9U) % kBlockWords] + sigma1;
    words_[slot] = valid ? next : words_[slot];
    return words_[slot];
  }

  static void mix_round(const uint8_t round, uint32_t* const work, const uint32_t scheduled) {
    const bool valid = check(work != nullptr) && check(round < kRounds);
    if (valid) {
      const uint32_t sum1 =
          rotate_right<6U>(work[4]) ^ rotate_right<11U>(work[4]) ^ rotate_right<25U>(work[4]);
      const uint32_t choose = (work[4] & work[5]) ^ ((~work[4]) & work[6]);
      const uint32_t constant = pgm_read_dword(&round_constants()[round]);
      const uint32_t first = work[7] + sum1 + choose + constant + scheduled;
      const uint32_t sum0 =
          rotate_right<2U>(work[0]) ^ rotate_right<13U>(work[0]) ^ rotate_right<22U>(work[0]);
      const uint32_t majority = (work[0] & work[1]) ^ (work[0] & work[2]) ^ (work[1] & work[2]);
      for (uint8_t i = static_cast<uint8_t>(kStateWords - 1U); i > 0U; --i) {
        work[i] = work[i - 1U];
      }
      work[4] = work[4] + first;
      work[0] = first + sum0 + majority;
    }
  }

  void compress() {
    const bool full = check((used_ == kSha256BlockSize) || (used_ == kLengthOffset));
    const bool open = check(!finished_);
    uint32_t work[kStateWords] = {};
    for (uint8_t i = 0U; i < kStateWords; ++i) {
      work[i] = state_[i];
    }
    for (uint8_t round = 0U; round < kRounds; ++round) {
      const uint32_t scheduled = (round < kBlockWords) ? words_[round] : next_schedule(round);
      mix_round(round, &work[0], scheduled);
    }
    for (uint8_t i = 0U; (full && open) && (i < kStateWords); ++i) {
      state_[i] = state_[i] + work[i];
    }
  }

  void write_digest(uint8_t* const digest) const {
    const bool valid = check(digest != nullptr) && check(!finished_);
    if (valid) {
      for (uint8_t i = 0U; i < kSha256Size; ++i) {
        const uint8_t shift = static_cast<uint8_t>(24U - ((i % kBytesPerWord) * 8U));
        digest[i] = static_cast<uint8_t>(state_[i / kBytesPerWord] >> shift);
      }
    }
  }

  uint32_t state_[kStateWords] = {};
  uint32_t words_[kBlockWords] = {};
  uint8_t used_ = 0U;
  uint32_t length_ = 0U;
  bool finished_ = false;
};

}

#endif
