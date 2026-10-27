#ifndef MADO_TEST_FAKE_DATA_H
#define MADO_TEST_FAKE_DATA_H

#include <stdint.h>

#include <random>
#include <string>

class FakeData {
 public:
  explicit FakeData(uint32_t seed) : engine_(seed) {
  }

  std::string word(size_t length) {
    static const char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::uniform_int_distribution<size_t> pick(0U, sizeof(kAlphabet) - 2U);
    std::string result;
    for (size_t i = 0U; i < length; ++i) {
      result.push_back(kAlphabet[pick(engine_)]);
    }
    return result;
  }

  uint32_t number(uint32_t min, uint32_t max) {
    std::uniform_int_distribution<uint32_t> pick(min, max);
    return pick(engine_);
  }

 private:
  std::mt19937 engine_;
};

#endif
