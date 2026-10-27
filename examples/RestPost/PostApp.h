// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef POST_APP_H
#define POST_APP_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/SlotTable.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>

#include "generated_api.h"

constexpr uint8_t kPostSlots = 4U;
constexpr uint32_t kPostMaxBody = 256U;

struct PostState {
  mado::SlotTable<rest_post::Item, kPostSlots> items;
};

template <typename ClientT>
struct PostApp {
  using Exchange = mado::Exchange<ClientT, rest_post::Limits>;

  static PostState& state() {
    static PostState instance = {};
    return instance;
  }

  static void begin() {
    state() = PostState{};
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    const bool known = mado::check(route <= rest_post::kStaticAsset);
    if (!known) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    return (route == rest_post::kCreateItem) ? create_item(exchange)
                                             : rest_post::send_static_asset(exchange);
  }

 private:
  static bool create_item(Exchange& exchange) {
    rest_post::Item item = {};
    uint8_t present = 0U;
    const mado::Problem problem = rest_post::read_item_body(exchange, item, present, kPostMaxBody);
    if (problem.status != mado::http::kNoResponse) {
      return mado::send_problem(exchange, problem);
    }
    const uint8_t slot = state().items.insert(item);
    rest_post::Item* const stored = state().items.find(slot);
    if (stored == nullptr) {
      return mado::send_problem(
          exchange, mado::Problem{mado::http::kInsufficientStorage, F("every item slot is taken")});
    }
    stored->id = slot;
    mado::Reply<Exchange> reply(exchange);
    const bool head =
        reply.status(mado::http::kCreated).json().no_store().location(F("/api/items/"), slot).end();
    return head && rest_post::write_item(exchange, *stored);
  }
};

#endif
