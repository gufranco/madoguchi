#ifndef MADO_TEST_FAKE_UDP_H
#define MADO_TEST_FAKE_UDP_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

class FakeUdp {
 public:
  void deliver(std::vector<uint8_t> packet) {
    inbox_.push_back(std::move(packet));
  }

  int parsePacket() {
    if (next_ >= inbox_.size()) {
      return 0;
    }
    current_ = inbox_[next_];
    ++next_;
    position_ = 0U;
    return static_cast<int>(current_.size());
  }

  int read() {
    if (position_ >= current_.size()) {
      return -1;
    }
    const uint8_t value = current_[position_];
    ++position_;
    return value;
  }

  int beginPacket(const IPAddress& address, uint16_t destination_port) {
    destination_ = address;
    port_ = destination_port;
    sending_.clear();
    return refuse_begin_ ? 0 : 1;
  }

  size_t write(uint8_t value) {
    if (written_ >= write_budget_) {
      return 0U;
    }
    ++written_;
    sending_.push_back(value);
    return 1U;
  }

  int endPacket() {
    if (refuse_end_) {
      return 0;
    }
    sent_.push_back(sending_);
    return 1;
  }

  void refuse_begin() {
    refuse_begin_ = true;
  }

  void limit_writes(size_t bytes) {
    write_budget_ = bytes;
  }

  void refuse_end() {
    refuse_end_ = true;
  }

  const std::vector<std::vector<uint8_t>>& sent() const {
    return sent_;
  }

  const IPAddress& destination() const {
    return destination_;
  }

  uint16_t port() const {
    return port_;
  }

 private:
  std::vector<std::vector<uint8_t>> inbox_;
  size_t next_ = 0U;
  std::vector<uint8_t> current_;
  size_t position_ = 0U;
  std::vector<uint8_t> sending_;
  std::vector<std::vector<uint8_t>> sent_;
  IPAddress destination_;
  uint16_t port_ = 0U;
  bool refuse_begin_ = false;
  bool refuse_end_ = false;
  size_t written_ = 0U;
  size_t write_budget_ = SIZE_MAX;
};

#endif
