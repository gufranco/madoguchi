// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef GET_APP_H
#define GET_APP_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/SlotTable.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>
#include <string.h>

#include "generated_api.h"

constexpr uint8_t kGetSlots = 4U;

struct GetState {
  mado::SlotTable<rest_get::Item, kGetSlots> items;
};

template <typename ClientT, typename SdT, uint16_t kReadMode>
struct GetApp {
  using Exchange = mado::Exchange<ClientT, rest_get::Limits>;

  static SdT*& card() {
    static SdT* instance = nullptr;
    return instance;
  }

  static GetState& state() {
    static GetState instance = {};
    return instance;
  }

  static void begin(SdT& sd, const uint8_t chip_select) {
    state() = GetState{};
    card() = sd.begin(chip_select) ? &sd : nullptr;
    seed("lamp", true);
    seed("fan", false);
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    const bool known = mado::check(route <= rest_get::kCardFile);
    if (!known) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    switch (route) {
      case rest_get::kListItems: return list_items(exchange);
      case rest_get::kGetItem: return get_item(exchange, route);
      default: return rest_get::send_card<kReadMode>(exchange, card(), route);
    }
  }

 private:
  static void seed(const char* const name, const bool on) {
    rest_get::Item item = {};
    strncpy(&item.name[0], name, sizeof(item.name) - 1U);
    item.on = on;
    const uint8_t slot = state().items.insert(item);
    rest_get::Item* const stored = state().items.find(slot);
    const bool placed = mado::check(stored != nullptr);
    if (placed) {
      stored->id = slot;
    }
  }

  static bool send_json_head(Exchange& exchange) {
    mado::Reply<Exchange> reply(exchange);
    return reply.status(mado::http::kOk).json().no_store().end();
  }

  static bool list_items(Exchange& exchange) {
    bool written = send_json_head(exchange) && (exchange.print(F("[")) > 0U);
    bool first = true;
    for (uint8_t slot = 1U; written && (slot <= kGetSlots); ++slot) {
      const rest_get::Item* const item = state().items.find(slot);
      if (item == nullptr) {
        continue;
      }
      written = (first || (exchange.print(F(",")) > 0U)) && rest_get::write_item(exchange, *item);
      first = false;
    }
    return written && (exchange.print(F("]")) > 0U);
  }

  static bool get_item(Exchange& exchange, const uint8_t route) {
    uint8_t slot = 0U;
    const bool parsed = rest_get::route_path_id(exchange.path(), route, slot);
    const bool in_table = parsed && mado::check(slot > 0U) && (slot <= kGetSlots);
    const rest_get::Item* const item = in_table ? state().items.find(slot) : nullptr;
    if (item == nullptr) {
      return mado::send_problem(exchange,
                                mado::Problem{mado::http::kNotFound, F("no item has this id")});
    }
    return send_json_head(exchange) && rest_get::write_item(exchange, *item);
  }
};

#endif
