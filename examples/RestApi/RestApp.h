// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_APP_H
#define REST_APP_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/FileReply.h>
#include <mado/LogEntry.h>
#include <mado/LoginGuard.h>
#include <mado/MemorySink.h>
#include <mado/Pbkdf2Params.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/ResponseSummary.h>
#include <mado/SdLog.h>
#include <mado/SlotTable.h>
#include <mado/TokenClaims.h>
#include <mado/TokenStatus.h>
#include <mado/UptimeClock.h>
#include <mado/check.h>
#include <mado/constant_time_equal.h>
#include <mado/http_status.h>
#include <mado/issue_token.h>
#include <mado/kMaxTokenClaimValue.h>
#include <mado/kSha256Size.h>
#include <mado/key_provisioned.h>
#include <mado/pbkdf2_sha256.h>
#include <mado/send_bearer_challenge.h>
#include <mado/send_file.h>
#include <mado/send_problem.h>
#include <mado/verify_bearer.h>
#include <stdint.h>
#include <string.h>

#include "RestStore.h"
#include "generated_api.h"

constexpr uint8_t kItemSlots = 4U;
constexpr uint32_t kTokenLifetime = 900U;
constexpr uint32_t kMaxRequestBody = 256U;
constexpr uint8_t kSaltSize = 16U;

constexpr uint32_t kLogMaxBytes = 65536UL;
const char kRestHostName[] PROGMEM = "madoguchi";
const char kRestLogPath[] PROGMEM = "/LOG.TXT";
const char kRestLogOldPath[] PROGMEM = "/LOG.OLD";

template <typename SdT, uint8_t kReadMode, uint8_t kWriteMode>
struct RestState {
  mado::SlotTable<rest_api::Item, kItemSlots> items;
  mado::UptimeClock clock;
  mado::LoginGuard guard;
  uint32_t session = 0U;
  uint32_t requests = 0U;
  SdT* sd = nullptr;
  RestStore<SdT, kReadMode, kWriteMode> store;
  mado::SdLog<SdT, kReadMode, kWriteMode> log{&kRestLogPath[0], &kRestLogOldPath[0], kLogMaxBytes};
};

template <typename ClientT, typename SecretsT, typename SdT, uint8_t kReadMode, uint8_t kWriteMode>
struct RestApp {
  using Exchange = mado::Exchange<ClientT, rest_api::Limits>;
  using State = RestState<SdT, kReadMode, kWriteMode>;

  static State& state() {
    static State instance = {};
    return instance;
  }

  static void begin(const uint32_t session, SdT& sd, const uint8_t chip_select) {
    const bool signable = mado::check(session <= mado::kMaxTokenClaimValue);
    state() = State{};
    state().session = signable ? session : 0U;
    const bool ready = sd.begin(chip_select);
    state().sd = ready ? &sd : nullptr;
    state().store.attach(state().sd);
    state().log.attach(state().sd);
    static_cast<void>(state().store.load_items(state().items));
    state().guard.restore(state().store.load_failures());
  }

  static void tick(const uint32_t now_ms) {
    const bool sound = mado::check(state().items.count() <= kItemSlots);
    if (sound) {
      state().clock.advance(now_ms);
    }
  }

  static void log_response(const mado::RequestLine& request,
                           const mado::ResponseSummary& response) {
    const uint32_t elapsed = millis() - response.started_ms;
    const bool sane = mado::check(elapsed < 0x80000000UL);
    state().requests = state().requests + 1U;
    const mado::RequestLog entry = {state().requests, request.method,
                                    (request.path == nullptr) ? "" : request.path, response.status,
                                    sane ? elapsed : 0U};
    const bool logged = state().log.request(stamp(), entry);
    static_cast<void>(logged);
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    tick(millis());
    const bool known = mado::check(route <= rest_api::kStaticAsset);
    if (!known) {
      return mado::send_problem(exchange, mado::Problem{mado::http::kNotFound, nullptr});
    }
    if (rest_api::route_needs_token(route) && (!authorized(exchange))) {
      return true;
    }
    return dispatch(exchange, route);
  }

 private:
  static bool dispatch(Exchange& exchange, const uint8_t route) {
    switch (route) {
      case rest_api::kLogin: return login(exchange);
      case rest_api::kListItems: return list_items(exchange);
      case rest_api::kCreateItem: return create_item(exchange);
      case rest_api::kGetItem: return get_item(exchange, route);
      case rest_api::kReplaceItem: return write_item_fields(exchange, route, true);
      case rest_api::kUpdateItem: return write_item_fields(exchange, route, false);
      case rest_api::kDeleteItem: return delete_item(exchange, route);
      case rest_api::kReadLog: return read_log(exchange);
      case rest_api::kClearLog: return clear_log(exchange);
      default: return rest_api::send_static_asset(exchange);
    }
  }

  static mado::LogStamp stamp() {
    const mado::LogStamp value = {state().session, millis(), F("madoguchi")};
    return value;
  }

  static bool storage_missing(Exchange& exchange) {
    return mado::send_problem(exchange, mado::Problem{mado::http::kServiceUnavailable,
                                                      F("the SD card is not available")});
  }

  static bool read_log(Exchange& exchange) {
    char path[mado::kLogPathSize] = {};
    const bool named = (state().sd != nullptr) && state().log.path(path);
    if ((!named) || (!state().sd->exists(&path[0]))) {
      return storage_missing(exchange);
    }
    auto file = state().sd->open(&path[0], kReadMode);
    const mado::FileReply reply = {static_cast<bool>(file) ? file.size() : 0U,
                                   F("application/x-ndjson"),
                                   false,
                                   nullptr,
                                   nullptr,
                                   F("no-store")};
    const bool sent = static_cast<bool>(file) ? mado::send_file(exchange, file, reply)
                                              : storage_missing(exchange);
    file.close();
    return sent;
  }

  static bool clear_log(Exchange& exchange) {
    const bool cleared = (state().sd != nullptr) && state().log.clear();
    return cleared ? exchange.send_empty(mado::http::kNoContent) : storage_missing(exchange);
  }

  static void remember_failures(const bool succeeded) {
    const bool saved = state().store.save_failures(state().guard.failures());
    const bool logged = state().log.event(stamp(), succeeded ? mado::kLogInfo : mado::kLogWarn,
                                          succeeded ? F("login ok") : F("login failed"));
    static_cast<void>(saved);
    static_cast<void>(logged);
  }

  static bool authorized(Exchange& exchange) {
    mado::TokenClaims claims = {};
    const mado::TokenStatus status =
        mado::verify_bearer(exchange.header(F("Authorization")), SecretsT::token_key(),
                            state().session, state().clock.seconds(), claims);
    const bool valid = mado::check(claims.ends_at <= mado::kMaxTokenClaimValue);
    if (valid && (status == mado::TokenStatus::kValid)) {
      return true;
    }
    const bool sent = mado::send_bearer_challenge(exchange, status);
    static_cast<void>(sent);
    return false;
  }

  static bool send_json_head(Exchange& exchange, const uint16_t code) {
    const bool success = mado::check((code == mado::http::kOk) || (code == mado::http::kCreated));
    mado::Reply<Exchange> reply(exchange);
    return success && reply.status(code).json().no_store().end();
  }

  static bool send_item(Exchange& exchange, const uint16_t code, const rest_api::Item& item) {
    const bool head = send_json_head(exchange, code);
    return head && rest_api::write_item(exchange, item);
  }

  static bool login(Exchange& exchange) {
    const uint32_t now = state().clock.seconds();
    if (!mado::key_provisioned(SecretsT::token_key())) {
      return mado::send_problem(exchange, mado::Problem{mado::http::kServiceUnavailable,
                                                        F("the token key is not provisioned")});
    }
    if (state().guard.locked(now)) {
      return send_locked(exchange, now);
    }
    rest_api::Credentials credentials = {};
    uint8_t present = 0U;
    const mado::Problem problem =
        rest_api::read_credentials_body(exchange, credentials, present, kMaxRequestBody);
    if (problem.status != mado::http::kNoResponse) {
      return mado::send_problem(exchange, problem);
    }
    if (!credentials_match(credentials)) {
      state().guard.record_failure(now);
      remember_failures(false);
      return mado::send_bearer_challenge(exchange, mado::TokenStatus::kMissing);
    }
    state().guard.record_success();
    remember_failures(true);
    return send_token(exchange, credentials, now);
  }

  static bool send_locked(Exchange& exchange, const uint32_t now) {
    const uint32_t wait = state().guard.retry_after(now);
    const bool waiting =
        mado::check(wait > 0U) && mado::check(wait <= mado::LoginGuard::kLockSeconds);
    mado::Reply<Exchange> reply(exchange);
    return reply.status(mado::http::kTooManyRequests)
        .header(F("Retry-After"), waiting ? wait : 1U)
        .length(0U)
        .end();
  }

  static uint8_t copy_bytes(const char* const text, uint8_t* const out, const uint8_t size) {
    const bool valid = mado::check(text != nullptr) && mado::check(out != nullptr);
    uint8_t length = 0U;
    for (; valid && (length < size) && (text[length] != '\0'); ++length) {
      out[length] = static_cast<uint8_t>(text[length]);
    }
    const bool terminated = mado::check(length < size);
    return terminated ? length : 0U;
  }

  static bool user_matches(const rest_api::Credentials& credentials) {
    char stored[sizeof(credentials.username)] = {};
    strncpy_P(&stored[0], SecretsT::user_name(), sizeof(stored) - 1U);
    uint8_t expected[sizeof(stored)] = {};
    uint8_t given[sizeof(stored)] = {};
    const uint8_t stored_length = copy_bytes(&stored[0], &expected[0], sizeof(expected));
    const uint8_t given_length = copy_bytes(&credentials.username[0], &given[0], sizeof(given));
    const bool sized = mado::check(stored_length > 0U) && (given_length > 0U);
    return mado::constant_time_equal(&expected[0], &given[0], sizeof(expected)) && sized;
  }

  static bool password_matches(const rest_api::Credentials& credentials) {
    uint8_t password[sizeof(credentials.password)] = {};
    uint8_t salt[kSaltSize] = {};
    uint8_t expected[mado::kSha256Size] = {};
    uint8_t derived[mado::kSha256Size] = {};
    const uint8_t length = copy_bytes(&credentials.password[0], &password[0], sizeof(password));
    memcpy_P(&salt[0], SecretsT::password_salt(), sizeof(salt));
    memcpy_P(&expected[0], SecretsT::password_hash(), sizeof(expected));
    const mado::Pbkdf2Params params = {&password[0], length, &salt[0], sizeof(salt),
                                       SecretsT::password_iterations()};
    const bool derived_ok = mado::check(length > 0U) && mado::pbkdf2_sha256(params, &derived[0]);
    return mado::constant_time_equal(&expected[0], &derived[0], sizeof(derived)) && derived_ok;
  }

  static bool credentials_match(const rest_api::Credentials& credentials) {
    const bool same_user = user_matches(credentials);
    const bool same_password = password_matches(credentials);
    return same_user && same_password;
  }

  static bool send_token(Exchange& exchange, const rest_api::Credentials& credentials,
                         const uint32_t now) {
    mado::TokenClaims claims = {};
    strncpy(&claims.subject[0], &credentials.username[0], sizeof(claims.subject) - 1U);
    claims.session = state().session;
    claims.ends_at = now + kTokenLifetime;
    rest_api::Token token = {};
    mado::MemorySink sink(&token.access_token[0], sizeof(token.access_token));
    const bool signable = mado::check(claims.ends_at <= mado::kMaxTokenClaimValue);
    const bool issued = signable && mado::issue_token(sink, SecretsT::token_key(), claims);
    strncpy_P(&token.token_type[0], PSTR("Bearer"), sizeof(token.token_type) - 1U);
    token.expires_in = static_cast<int32_t>(kTokenLifetime);
    const bool head = issued && send_json_head(exchange, mado::http::kOk);
    return head && rest_api::write_token(exchange, token);
  }

  static bool list_items(Exchange& exchange) {
    const bool head = send_json_head(exchange, mado::http::kOk);
    bool written = head && (exchange.print(F("[")) > 0U);
    bool first = true;
    for (uint8_t slot = 1U; written && (slot <= kItemSlots); ++slot) {
      const rest_api::Item* const item = state().items.find(slot);
      if (item == nullptr) {
        continue;
      }
      written = (first || (exchange.print(F(",")) > 0U)) && rest_api::write_item(exchange, *item);
      first = false;
    }
    return written && (exchange.print(F("]")) > 0U);
  }

  static mado::Problem read_patch(Exchange& exchange, rest_api::Item& item) {
    rest_api::ItemPatch patch = {};
    uint8_t present = 0U;
    const mado::Problem problem =
        rest_api::read_item_patch_body(exchange, patch, present, kMaxRequestBody);
    const bool sound = mado::check(present <= rest_api::kItemPatchWritable);
    if ((problem.status != mado::http::kNoResponse) || (!sound)) {
      return problem;
    }
    if (present == 0U) {
      return mado::Problem{mado::http::kUnprocessableContent, F("the body changes no property")};
    }
    apply_patch(patch, present, item);
    return problem;
  }

  static void apply_patch(const rest_api::ItemPatch& patch, const uint8_t present,
                          rest_api::Item& item) {
    static_assert(sizeof(patch.name) == sizeof(item.name), "both schemas share the name limit");
    const bool sound = mado::check(present <= rest_api::kItemPatchWritable);
    if (sound && ((present & rest_api::kItemPatchName) != 0U)) {
      memcpy(&item.name[0], &patch.name[0], sizeof(item.name));
    }
    item.value = ((present & rest_api::kItemPatchValue) != 0U) ? patch.value : item.value;
    item.on = ((present & rest_api::kItemPatchOn) != 0U) ? patch.on : item.on;
  }

  static void warn_unsaved(const bool saved) {
    if (!saved) {
      const bool logged =
          state().log.event(stamp(), mado::kLogWarn, F("the SD card refused a save"));
      static_cast<void>(logged);
    }
  }

  static void persist(const rest_api::Item& item) {
    const bool valid = mado::check(item.id > 0) && mado::check(item.id <= kItemSlots);
    const bool saved = (state().sd == nullptr) || (valid && state().store.save_item(item));
    warn_unsaved(saved);
  }

  static bool send_missing(Exchange& exchange) {
    return mado::send_problem(exchange,
                              mado::Problem{mado::http::kNotFound, F("no item has this id")});
  }

  static bool create_item(Exchange& exchange) {
    rest_api::Item item = {};
    uint8_t present = 0U;
    const mado::Problem problem =
        rest_api::read_item_body(exchange, item, present, kMaxRequestBody);
    if (problem.status != mado::http::kNoResponse) {
      return mado::send_problem(exchange, problem);
    }
    const uint8_t slot = state().items.insert(item);
    rest_api::Item* const stored = state().items.find(slot);
    if (stored == nullptr) {
      return mado::send_problem(
          exchange, mado::Problem{mado::http::kInsufficientStorage, F("every item slot is taken")});
    }
    stored->id = slot;
    item = *stored;
    persist(item);
    mado::Reply<Exchange> reply(exchange);
    const bool head = reply.status(mado::http::kCreated)
                          .json()
                          .no_store()
                          .location(rest_api::get_item_url(), slot)
                          .end();
    return head && rest_api::write_item(exchange, item);
  }

  static uint8_t item_slot(const Exchange& exchange, const uint8_t route) {
    uint8_t slot = 0U;
    const bool parsed = rest_api::route_path_id(exchange.path(), route, slot);
    const bool in_table = parsed && mado::check(slot > 0U) && (slot <= kItemSlots);
    return (in_table && (state().items.find(slot) != nullptr)) ? slot : 0U;
  }

  static bool get_item(Exchange& exchange, const uint8_t route) {
    const uint8_t slot = item_slot(exchange, route);
    const rest_api::Item* const item = state().items.find(slot);
    if ((slot == 0U) || (item == nullptr)) {
      return send_missing(exchange);
    }
    return send_item(exchange, mado::http::kOk, *item);
  }

  static bool write_item_fields(Exchange& exchange, const uint8_t route, const bool replace) {
    const uint8_t slot = item_slot(exchange, route);
    rest_api::Item* const item = state().items.find(slot);
    if ((slot == 0U) || (item == nullptr)) {
      return send_missing(exchange);
    }
    rest_api::Item update = *item;
    uint8_t present = 0U;
    const mado::Problem problem =
        replace ? rest_api::read_item_body(exchange, update, present, kMaxRequestBody)
                : read_patch(exchange, update);
    if (problem.status != mado::http::kNoResponse) {
      return mado::send_problem(exchange, problem);
    }
    update.id = slot;
    *item = update;
    persist(update);
    return send_item(exchange, mado::http::kOk, update);
  }

  static bool delete_item(Exchange& exchange, const uint8_t route) {
    const uint8_t slot = item_slot(exchange, route);
    if (slot == 0U) {
      return send_missing(exchange);
    }
    const bool erased = state().items.erase(slot);
    const bool forgotten = (state().sd == nullptr) || state().store.remove_item(slot);
    warn_unsaved(forgotten);
    return erased ? exchange.send_empty(mado::http::kNoContent)
                  : mado::send_problem(exchange,
                                       mado::Problem{mado::http::kInternalServerError, nullptr});
  }
};

#endif
