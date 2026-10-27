#ifndef MADO_TEST_FAKE_SD_H
#define MADO_TEST_FAKE_SD_H

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <utility>

constexpr uint8_t FILE_READ = 1U;
constexpr uint8_t FILE_WRITE = 2U;

class FakeSd;

class FakeSdFile {
 public:
  FakeSdFile() = default;
  FakeSdFile(FakeSd* sd, std::string path, bool directory);

  explicit operator bool() const {
    return sd_ != nullptr;
  }

  int read(uint8_t* buffer, uint16_t length);
  size_t write(const uint8_t* buffer, size_t length);
  uint32_t size();
  bool seek(uint32_t position);
  const char* name();
  FakeSdFile openNextFile();

  bool isDirectory() {
    directory_ = directory_ || (path_ == "/");
    return directory_;
  }

  void close() {
    sd_ = nullptr;
  }

 private:
  FakeSd* sd_ = nullptr;
  std::string path_;
  std::string name_;
  bool directory_ = false;
  size_t position_ = 0U;
  size_t listed_ = 0U;
};

class FakeSd {
 public:
  bool begin(uint8_t chip_select) {
    chip_select_ = chip_select;
    ++begins_;
    return present_;
  }

  FakeSdFile open(const char* path, uint8_t mode = FILE_READ) {
    const std::string name(path);
    if (name == "/") {
      return present_ ? FakeSdFile(this, name, true) : FakeSdFile();
    }
    const bool known = files_.count(name) == 1U;
    if ((!present_) || ((!known) && (mode != FILE_WRITE))) {
      return FakeSdFile();
    }
    if (!known) {
      files_[name] = "";
    }
    return FakeSdFile(this, name, false);
  }

  bool exists(const char* path) const {
    return files_.count(path) == 1U;
  }

  bool remove(const char* path) {
    return refuse_remove_ ? false : (files_.erase(path) == 1U);
  }

  void set_present(bool present) {
    present_ = present;
  }

  void limit_writes(size_t bytes) {
    write_budget_ = bytes;
  }

  void refuse_remove() {
    refuse_remove_ = true;
  }

  std::map<std::string, std::string>& files() {
    return files_;
  }

  size_t& write_budget() {
    return write_budget_;
  }

  uint32_t begins() const {
    return begins_;
  }

 private:
  std::map<std::string, std::string> files_;
  bool present_ = true;
  bool refuse_remove_ = false;
  size_t write_budget_ = SIZE_MAX;
  uint8_t chip_select_ = 0U;
  uint32_t begins_ = 0U;
};

inline FakeSdFile::FakeSdFile(FakeSd* sd, std::string path, bool directory)
    : sd_(sd), path_(std::move(path)), directory_(directory) {
}

inline int FakeSdFile::read(uint8_t* buffer, uint16_t length) {
  const std::string& content = sd_->files()[path_];
  const size_t count =
      std::min<size_t>(length, content.size() - std::min(position_, content.size()));
  std::copy_n(content.begin() + static_cast<std::ptrdiff_t>(position_), count, buffer);
  position_ += count;
  return (count == 0U) ? -1 : static_cast<int>(count);
}

inline size_t FakeSdFile::write(const uint8_t* buffer, size_t length) {
  size_t& budget = sd_->write_budget();
  const size_t accepted = std::min(length, budget);
  budget -= (budget == SIZE_MAX) ? 0U : accepted;
  sd_->files()[path_].append(reinterpret_cast<const char*>(buffer), accepted);
  return accepted;
}

inline uint32_t FakeSdFile::size() {
  return directory_ ? 0U : static_cast<uint32_t>(sd_->files()[path_].size());
}

inline bool FakeSdFile::seek(uint32_t position) {
  const bool inside = position <= size();
  position_ = inside ? position : position_;
  return inside;
}

inline const char* FakeSdFile::name() {
  name_ = (path_.size() > 1U) ? path_.substr(1U) : path_;
  return name_.c_str();
}

inline FakeSdFile FakeSdFile::openNextFile() {
  const std::map<std::string, std::string>& files = sd_->files();
  if (listed_ >= files.size()) {
    return FakeSdFile();
  }
  const auto entry = std::next(files.begin(), static_cast<std::ptrdiff_t>(listed_));
  ++listed_;
  return FakeSdFile(sd_, entry->first, false);
}

#endif
