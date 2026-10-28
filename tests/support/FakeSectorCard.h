#ifndef MADO_TEST_FAKE_SECTOR_CARD_H
#define MADO_TEST_FAKE_SECTOR_CARD_H

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <map>

constexpr size_t kFakeSectorSize = 512U;
constexpr uint8_t kFakeUnwritten = 0x5AU;

class FakeSectorCard {
 public:
  using Sector = std::array<uint8_t, kFakeSectorSize>;

  FakeSectorCard(uint32_t sectors, uint8_t type) : sectors_(sectors), type_(type) {
  }

  uint32_t sectorCount() const {
    return sectors_;
  }

  uint8_t type() const {
    return type_;
  }

  uint8_t errorCode() const {
    return present_ ? 0U : 1U;
  }

  bool readSectors(uint32_t sector, uint8_t* destination, size_t count) {
    if ((!present_) || (sector + count > sectors_)) {
      return false;
    }
    for (size_t i = 0U; i < count; ++i) {
      const Sector stored = read(static_cast<uint32_t>(sector + i));
      std::copy(stored.begin(), stored.end(), destination + (i * kFakeSectorSize));
    }
    reads_ += count;
    return true;
  }

  bool writeSectors(uint32_t sector, const uint8_t* source, size_t count) {
    if ((!present_) || (sector + count > sectors_) || (sector + count > refuse_from_)) {
      return false;
    }
    for (size_t i = 0U; i < count; ++i) {
      Sector& stored = sectors_written_[static_cast<uint32_t>(sector + i)];
      std::copy(source + (i * kFakeSectorSize), source + ((i + 1U) * kFakeSectorSize),
                stored.begin());
    }
    writes_ += count;
    return true;
  }

  Sector read(uint32_t sector) const {
    Sector stored = {};
    const auto found = sectors_written_.find(sector);
    if (found == sectors_written_.end()) {
      stored.fill(kFakeUnwritten);
    } else {
      stored = found->second;
    }
    if (sector == stuck_sector_) {
      stored[stuck_offset_] = stuck_value_;
    }
    return stored;
  }

  void set_present(bool present) {
    present_ = present;
  }

  void resize(uint32_t sectors) {
    sectors_ = sectors;
  }

  void refuse_writes_from(uint32_t sector) {
    refuse_from_ = sector;
  }

  void stick(uint32_t sector, size_t offset, uint8_t value) {
    stuck_sector_ = sector;
    stuck_offset_ = offset;
    stuck_value_ = value;
  }

  size_t reads() const {
    return reads_;
  }

  size_t writes() const {
    return writes_;
  }

 private:
  uint32_t sectors_;
  uint8_t type_;
  bool present_ = true;
  uint32_t refuse_from_ = UINT32_MAX;
  uint32_t stuck_sector_ = UINT32_MAX;
  size_t stuck_offset_ = 0U;
  uint8_t stuck_value_ = 0U;
  size_t reads_ = 0U;
  size_t writes_ = 0U;
  std::map<uint32_t, Sector> sectors_written_;
};

#endif
