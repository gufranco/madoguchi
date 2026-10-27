// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_MDNS_RESPONDER_H
#define MADO_MDNS_RESPONDER_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"
#include "to_lower_ascii.h"

namespace mado {

constexpr uint16_t kMdnsPort = 5353U;
constexpr uint8_t kMdnsMaxQuestions = 4U;
constexpr uint8_t kMdnsMaxLabel = 63U;
constexpr uint32_t kMdnsTtlSeconds = 120UL;
constexpr uint16_t kMdnsTypeA = 1U;
constexpr uint16_t kMdnsTypeAny = 255U;
constexpr uint16_t kMdnsClassIn = 1U;

template <typename UdpT, typename AddressT>
class MdnsResponder {
  static_assert(kMdnsMaxLabel <= 0x3FU, "a DNS label length uses the low six bits");
  static_assert(kMdnsMaxQuestions > 0U, "a query holds at least one question");

 public:
  explicit MdnsResponder(const char* const flash_name) : name_(flash_name) {
  }

  bool poll(UdpT& udp, const AddressT& address) const {
    const bool named = check(name_ != nullptr) && check(name_length() > 0U);
    const int32_t size = udp.parsePacket();
    if ((!named) || (size < kHeaderBytes)) {
      return false;
    }
    const uint16_t flags = read_word(udp, 2U);
    const uint16_t questions = read_word(udp, 0U);
    const bool query = (flags & kResponseFlag) == 0U;
    bool asked = false;
    skip(udp, kCountBytesAfterQuestions);
    for (uint16_t i = 0U; query && (i < questions) && (i < kMdnsMaxQuestions); ++i) {
      const bool ours = read_name(udp);
      const uint16_t type = read_word(udp, 0U);
      const uint16_t klass = static_cast<uint16_t>(read_word(udp, 0U) & kClassMask);
      const bool wanted = (type == kMdnsTypeA) || (type == kMdnsTypeAny);
      asked = asked || (ours && wanted && (klass == kMdnsClassIn));
    }
    return asked && answer(udp, address);
  }

 private:
  static constexpr int32_t kHeaderBytes = 12;
  static constexpr uint8_t kCountBytesAfterQuestions = 6U;
  static constexpr uint16_t kResponseFlag = 0x8000U;
  static constexpr uint16_t kClassMask = 0x7FFFU;
  static constexpr uint16_t kAuthoritativeAnswer = 0x8400U;
  static constexpr uint16_t kCacheFlushIn = 0x8001U;
  static constexpr uint8_t kPointerMask = 0xC0U;
  static constexpr uint8_t kMaxLabels = 8U;

  uint8_t name_length() const {
    const bool named = check(name_ != nullptr);
    const size_t length = named ? strlen_P(name_) : 0U;
    return check(length <= kMdnsMaxLabel) ? static_cast<uint8_t>(length) : 0U;
  }

  static uint8_t read_byte(UdpT& udp) {
    const int32_t value = udp.read();
    const bool in_range = check(value <= 0xFF) && check(value >= -1);
    return ((value >= 0) && in_range) ? static_cast<uint8_t>(value) : 0U;
  }

  static uint16_t read_word(UdpT& udp, const uint8_t skip_before) {
    const bool bounded = check(skip_before < static_cast<uint8_t>(kHeaderBytes));
    skip(udp, bounded ? skip_before : 0U);
    const uint16_t high = read_byte(udp);
    const uint16_t low = read_byte(udp);
    return static_cast<uint16_t>((static_cast<uint32_t>(high) << 8U) | low);
  }

  static void skip(UdpT& udp, const uint8_t count) {
    const bool bounded = check(count <= kMdnsMaxLabel);
    for (uint8_t i = 0U; bounded && (i < count); ++i) {
      static_cast<void>(read_byte(udp));
    }
  }

  bool read_name(UdpT& udp) const {
    bool matches = true;
    for (uint8_t label = 0U; label < kMaxLabels; ++label) {
      const uint8_t length = read_byte(udp);
      if (length == 0U) {
        return matches && (label == 2U);
      }
      if ((length & kPointerMask) != 0U) {
        static_cast<void>(read_byte(udp));
        return false;
      }
      const bool bounded = check(length <= kMdnsMaxLabel);
      const char* const expected = (label == 0U) ? name_ : PSTR("local");
      const bool sized = bounded && (label < 2U) && (length == strlen_P(expected));
      const bool same = sized && label_matches(udp, expected, length);
      if (!sized) {
        skip(udp, length);
      }
      matches = matches && same;
    }
    return false;
  }

  static bool label_matches(UdpT& udp, const char* const expected, const uint8_t length) {
    bool same = check(expected != nullptr) && check(length <= kMdnsMaxLabel);
    for (uint8_t i = 0U; i < length; ++i) {
      const char wanted = static_cast<char>(pgm_read_byte(expected + i));
      const char got = to_lower_ascii(static_cast<char>(read_byte(udp)));
      same = same && (to_lower_ascii(wanted) == got);
    }
    return same;
  }

  bool answer(UdpT& udp, const AddressT& address) const {
    const AddressT group(224U, 0U, 0U, 251U);
    const bool opened = check(udp.beginPacket(group, kMdnsPort) == 1);
    if (!opened) {
      return false;
    }
    bool written = write_word(udp, 0U) && write_word(udp, kAuthoritativeAnswer);
    written = written && write_word(udp, 0U) && write_word(udp, 1U);
    written = written && write_word(udp, 0U) && write_word(udp, 0U);
    written = written && write_label(udp, name_) && write_label(udp, PSTR("local"));
    written = written && check(udp.write(static_cast<uint8_t>(0U)) == 1U);
    written = written && write_word(udp, kMdnsTypeA) && write_word(udp, kCacheFlushIn);
    written = written && write_word(udp, static_cast<uint16_t>(kMdnsTtlSeconds >> 16U));
    written = written && write_word(udp, static_cast<uint16_t>(kMdnsTtlSeconds & 0xFFFFU));
    written = written && write_word(udp, 4U);
    const uint32_t octets = static_cast<uint32_t>(address);
    for (uint8_t i = 0U; written && (i < 4U); ++i) {
      written = check(udp.write(static_cast<uint8_t>((octets >> (8U * i)) & 0xFFU)) == 1U);
    }
    const bool sent = check(udp.endPacket() == 1);
    return written && sent;
  }

  static bool write_word(UdpT& udp, const uint16_t value) {
    const bool high = check(udp.write(static_cast<uint8_t>(value >> 8U)) == 1U);
    const bool low = check(udp.write(static_cast<uint8_t>(value & 0xFFU)) == 1U);
    return high && low;
  }

  static bool write_label(UdpT& udp, const char* const flash_label) {
    const size_t length = strlen_P(flash_label);
    const bool bounded = check(length <= kMdnsMaxLabel) && check(length > 0U);
    bool written = bounded && check(udp.write(static_cast<uint8_t>(length)) == 1U);
    for (size_t i = 0U; written && (i < length); ++i) {
      written = check(udp.write(pgm_read_byte(flash_label + i)) == 1U);
    }
    return written;
  }

  const char* name_;
};

}

#endif
