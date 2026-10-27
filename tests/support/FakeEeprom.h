#ifndef MADO_TEST_FAKE_EEPROM_H
#define MADO_TEST_FAKE_EEPROM_H

#include <stddef.h>
#include <stdint.h>

#include <vector>

class FakeEeprom {
 public:
  explicit FakeEeprom(const size_t size = 1024U) : cells_(size, 0xFFU) {
  }

  uint8_t read(const int index) const {
    return cells_.at(static_cast<size_t>(index));
  }

  void update(const int index, const uint8_t value) {
    uint8_t& cell = cells_.at(static_cast<size_t>(index));
    writes_ += (cell == value) ? 0U : 1U;
    cell = value;
  }

  uint16_t length() const {
    return static_cast<uint16_t>(cells_.size());
  }

  size_t writes() const {
    return writes_;
  }

 private:
  std::vector<uint8_t> cells_;
  size_t writes_ = 0U;
};

#endif
