#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <string>

#include "../support/TestSecrets.h"
#include "FuzzTarget.h"
#include "mado/Base64UrlDecoder.h"
#include "mado/DecodeMode.h"
#include "mado/TokenClaims.h"
#include "mado/TokenStatus.h"
#include "mado/find_query_param.h"
#include "mado/url_decode.h"
#include "mado/verify_bearer.h"

namespace {

constexpr size_t kMaxText = 1023U;
constexpr uint16_t kTargetSize = 64U;

void decode_both_ways(const std::string& text) {
  char target[kTargetSize] = {};
  const uint16_t length = static_cast<uint16_t>(text.size());
  const int16_t path =
      mado::url_decode(text.c_str(), length, &target[0], kTargetSize, mado::DecodeMode::kPath);
  const int16_t form =
      mado::url_decode(text.c_str(), length, &target[0], kTargetSize, mado::DecodeMode::kForm);
  if ((path >= static_cast<int16_t>(kTargetSize)) || (form >= static_cast<int16_t>(kTargetSize))) {
    std::abort();
  }
  const bool found = mado::find_query_param(text.c_str(), "name", &target[0], kTargetSize);
  static_cast<void>(found);
}

void decode_base64(const std::string& text) {
  mado::Base64UrlDecoder decoder;
  uint8_t out[3] = {};
  for (const char symbol : text) {
    const int8_t got = decoder.push(symbol, &out[0]);
    if (got > 3) {
      std::abort();
    }
  }
  const int8_t tail = decoder.finish(&out[0]);
  if (tail > 3) {
    std::abort();
  }
}

void verify(const std::string& text) {
  mado::TokenClaims claims = {};
  const mado::TokenStatus status =
      mado::verify_bearer(text.c_str(), test_secrets::TestSecrets::token_key(), 7U, 0U, claims);
  const bool terminated = claims.subject[sizeof(claims.subject) - 1U] == '\0';
  if ((!terminated) || ((status == mado::TokenStatus::kValid) && (claims.session != 7U))) {
    std::abort();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > kMaxText) {
    return 0;
  }
  const std::string text(reinterpret_cast<const char*>(data), size);
  if (text.find('\0') != std::string::npos) {
    return 0;
  }
  decode_both_ways(text);
  decode_base64(text);
  verify(text);
  return 0;
}
