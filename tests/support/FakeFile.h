#ifndef MADO_TEST_FAKE_FILE_H
#define MADO_TEST_FAKE_FILE_H

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>

class FakeFile {
 public:
  explicit FakeFile(std::string content, size_t fail_after = SIZE_MAX)
      : content_(std::move(content)), fail_after_(fail_after) {
  }

  int read(uint8_t* buffer, uint16_t size) {
    const size_t remaining = content_.size() - position_;
    const size_t allowed =
        (position_ >= fail_after_) ? 0U : std::min(remaining, fail_after_ - position_);
    const size_t count = std::min<size_t>(size, allowed);
    std::copy_n(content_.begin() + static_cast<std::ptrdiff_t>(position_), count, buffer);
    position_ += count;
    return count == 0U ? -1 : static_cast<int>(count);
  }

  bool seek(uint32_t position) {
    if (position > content_.size()) {
      return false;
    }
    position_ = position;
    return true;
  }

  size_t write(const uint8_t* buffer, size_t size) {
    const size_t room = (written_.size() >= fail_after_) ? 0U : (fail_after_ - written_.size());
    const size_t accepted = std::min(size, room);
    written_.append(reinterpret_cast<const char*>(buffer), accepted);
    return accepted;
  }

  explicit operator bool() const {
    return open_;
  }

  void close_file() {
    open_ = false;
  }

  const std::string& written() const {
    return written_;
  }

 private:
  std::string content_;
  size_t fail_after_;
  size_t position_ = 0U;
  std::string written_;
  bool open_ = true;
};

#endif
