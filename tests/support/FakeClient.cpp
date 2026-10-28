#include "FakeClient.h"

#include <algorithm>
#include <utility>

#include "FakeClock.h"

FakeClient::FakeClient(std::string input)
    : input_(std::move(input)),
      position_(0U),
      max_available_(SIZE_MAX),
      max_write_(SIZE_MAX),
      read_overstatement_(0),
      forced_single_read_(0),
      single_read_forced_(false),
      close_when_drained_(false),
      write_budget_(SIZE_MAX),
      connected_(true),
      stopped_(false),
      single_byte_reads_(0U),
      block_reads_(0U),
      write_calls_(0U),
      read_delay_ms_(0U) {
}

int FakeClient::available() const {
  const size_t remaining = input_.size() - position_;
  if (remaining == 0U) {
    fake_clock::advance(1U);
  }
  return static_cast<int>(std::min(remaining, max_available_));
}

int FakeClient::read() {
  ++single_byte_reads_;
  fake_clock::advance(read_delay_ms_);
  if (single_read_forced_) {
    ++position_;
    return forced_single_read_;
  }
  if (position_ >= input_.size()) {
    return -1;
  }
  const auto value = static_cast<uint8_t>(input_[position_]);
  ++position_;
  return value;
}

int FakeClient::read(uint8_t* buffer, size_t size) {
  ++block_reads_;
  fake_clock::advance(read_delay_ms_);
  const size_t remaining = input_.size() - position_;
  const size_t count = std::min({size, remaining, max_available_});
  std::copy_n(input_.begin() + static_cast<std::ptrdiff_t>(position_), count, buffer);
  position_ += count;
  return count == 0U ? -1 : static_cast<int>(count) + read_overstatement_;
}

size_t FakeClient::write(uint8_t value) {
  ++write_calls_;
  output_.push_back(static_cast<char>(value));
  return 1U;
}

size_t FakeClient::write(const uint8_t* buffer, size_t size) {
  ++write_calls_;
  const size_t accepted = std::min({size, max_write_, write_budget_});
  output_.append(reinterpret_cast<const char*>(buffer), accepted);
  write_budget_ -= (write_budget_ == SIZE_MAX) ? 0U : accepted;
  return accepted;
}

uint8_t FakeClient::connected() const {
  const bool drained = close_when_drained_ && (position_ >= input_.size());
  return (connected_ && (!drained)) ? 1U : 0U;
}

void FakeClient::stop() {
  stopped_ = true;
  connected_ = false;
}

void FakeClient::disconnect() {
  connected_ = false;
}

void FakeClient::limit_available(size_t max_bytes) {
  max_available_ = max_bytes;
}

void FakeClient::limit_write(size_t max_bytes) {
  max_write_ = max_bytes;
}

void FakeClient::overstate_block_reads(int extra) {
  read_overstatement_ = extra;
}

void FakeClient::fail_writes_after(size_t total_bytes) {
  write_budget_ = total_bytes;
}

void FakeClient::delay_each_read(uint32_t delay_ms) {
  read_delay_ms_ = delay_ms;
}

size_t FakeClient::unread() const {
  return input_.size() - position_;
}

void FakeClient::close_when_drained() {
  close_when_drained_ = true;
}

void FakeClient::force_single_read(int value) {
  forced_single_read_ = value;
  single_read_forced_ = true;
}

const std::string& FakeClient::output() const {
  return output_;
}

bool FakeClient::stopped() const {
  return stopped_;
}

uint32_t FakeClient::single_byte_reads() const {
  return single_byte_reads_;
}

uint32_t FakeClient::block_reads() const {
  return block_reads_;
}

uint32_t FakeClient::write_calls() const {
  return write_calls_;
}

IPAddress FakeClient::remoteIP() const {
  return remote_;
}

void FakeClient::set_remote(IPAddress address) {
  remote_ = address;
}
