#ifndef MADO_TEST_FAKE_SD_H
#define MADO_TEST_FAKE_SD_H

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <utility>

constexpr uint16_t FILE_READ = 0x0000U;
constexpr uint16_t FILE_WRITE = 0x4202U;

class FakeSd;

class FakeSdFile {
 public:
  FakeSdFile() = default;
  FakeSdFile(FakeSd* sd, std::string path, bool directory);

  explicit operator bool() const {
    return sd_ != nullptr;
  }

  int read(void* buffer, size_t length);
  size_t write(const uint8_t* buffer, size_t length);
  uint64_t size();
  bool seek(uint64_t position);
  size_t getName(char* name, size_t length) const;
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

  FakeSdFile open(const char* path, uint16_t mode = FILE_READ) {
    const std::string name = stored_name(path);
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
    return files_.count(stored_name(path)) == 1U;
  }

  bool remove(const char* path) {
    return refuse_remove_ ? false : (files_.erase(stored_name(path)) == 1U);
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

  void report_size(const std::string& path, uint64_t size) {
    reported_sizes_[path] = size;
  }

  uint64_t size_of(const std::string& path) {
    const auto reported = reported_sizes_.find(path);
    return (reported == reported_sizes_.end()) ? static_cast<uint64_t>(files_[path].size())
                                               : reported->second;
  }

  size_t& write_budget() {
    return write_budget_;
  }

  uint32_t begins() const {
    return begins_;
  }

 private:
  std::string stored_name(const char* path) const {
    const std::string wanted(path);
    const auto same = [&wanted](const std::pair<const std::string, std::string>& entry) {
      return (entry.first.size() == wanted.size()) &&
             std::equal(entry.first.begin(), entry.first.end(), wanted.begin(),
                        [](char left, char right) {
                          return std::tolower(static_cast<unsigned char>(left)) ==
                                 std::tolower(static_cast<unsigned char>(right));
                        });
    };
    const auto found = std::find_if(files_.begin(), files_.end(), same);
    return (found == files_.end()) ? wanted : found->first;
  }

  std::map<std::string, std::string> files_;
  std::map<std::string, uint64_t> reported_sizes_;
  bool present_ = true;
  bool refuse_remove_ = false;
  size_t write_budget_ = SIZE_MAX;
  uint8_t chip_select_ = 0U;
  uint32_t begins_ = 0U;
};

inline FakeSdFile::FakeSdFile(FakeSd* sd, std::string path, bool directory)
    : sd_(sd), path_(std::move(path)), directory_(directory) {
}

inline int FakeSdFile::read(void* buffer, size_t length) {
  const std::string& content = sd_->files()[path_];
  const size_t count =
      std::min<size_t>(length, content.size() - std::min(position_, content.size()));
  std::copy_n(content.begin() + static_cast<std::ptrdiff_t>(position_), count,
              static_cast<char*>(buffer));
  position_ += count;
  return static_cast<int>(count);
}

inline size_t FakeSdFile::write(const uint8_t* buffer, size_t length) {
  size_t& budget = sd_->write_budget();
  const size_t accepted = std::min(length, budget);
  budget -= (budget == SIZE_MAX) ? 0U : accepted;
  sd_->files()[path_].append(reinterpret_cast<const char*>(buffer), accepted);
  return accepted;
}

inline uint64_t FakeSdFile::size() {
  return directory_ ? 0U : sd_->size_of(path_);
}

inline bool FakeSdFile::seek(uint64_t position) {
  const bool inside = position <= size();
  position_ = inside ? static_cast<size_t>(position) : position_;
  return inside;
}

inline size_t FakeSdFile::getName(char* name, size_t length) const {
  const std::string stored = (path_.size() > 1U) ? path_.substr(path_.rfind('/') + 1U) : path_;
  const bool fits = (length > 0U) && (stored.size() < length);
  const size_t copied = fits ? stored.size() : 0U;
  std::copy_n(stored.begin(), copied, name);
  if (length > 0U) {
    name[copied] = '\0';
  }
  return copied;
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
