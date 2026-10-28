// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef PUT_APP_H
#define PUT_APP_H

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

constexpr uint8_t kPutSlots = 4U;
constexpr uint32_t kPutMaxBody = 256U;

struct PutState {
  mado::SlotTable<rest_put::Item, kPutSlots> items;
};

template <typename ClientT, typename SdT, uint16_t kReadMode>
struct PutApp {
  using Exchange = mado::Exchange<ClientT, rest_put::Limits>;

  static SdT*& card() {
    static SdT* instance = nullptr;
    return instance;
  }

  static PutState& state() {
    static PutState instance = {};
    return instance;
  }

  static void begin(SdT& sd, const uint8_t chip_select) {
    state() = PutState{};
    card() = sd.begin(chip_select) ? &sd : nullptr;
    seed("lamp", true);
    seed("fan", false);
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    const bool known = mado::check(route <= rest_put::kCardFile);
    if (!known) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    return (route == rest_put::kReplaceItem)
               ? replace_item(exchange, route)
               : rest_put::send_card<kReadMode>(exchange, card(), route);
  }

 private:
  static void seed(const char* const name, const bool on) {
    rest_put::Item item = {};
    strncpy(&item.name[0], name, sizeof(item.name) - 1U);
    item.on = on;
    const uint8_t slot = state().items.insert(item);
    rest_put::Item* const stored = state().items.find(slot);
    const bool placed = mado::check(stored != nullptr);
    if (placed) {
      stored->id = slot;
    }
  }

  static bool replace_item(Exchange& exchange, const uint8_t route) {
    uint8_t slot = 0U;
    const bool parsed = rest_put::route_path_id(exchange.path(), route, slot);
    const bool in_table = parsed && mado::check(slot > 0U) && (slot <= kPutSlots);
    rest_put::Item* const item = in_table ? state().items.find(slot) : nullptr;
    if (item == nullptr) {
      return mado::send_problem(exchange,
                                mado::Problem{mado::http::kNotFound, F("no item has this id")});
    }
    rest_put::Item update = *item;
    uint8_t present = 0U;
    const mado::Problem problem = rest_put::read_item_body(exchange, update, present, kPutMaxBody);
    if (problem.status != mado::http::kNoResponse) {
      return mado::send_problem(exchange, problem);
    }
    *item = update;
    mado::Reply<Exchange> reply(exchange);
    return reply.status(mado::http::kOk).json().no_store().end() &&
           rest_put::write_item(exchange, *item);
  }
};

#endif
