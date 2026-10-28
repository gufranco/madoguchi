// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef DELETE_APP_H
#define DELETE_APP_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/SlotTable.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>
#include <string.h>

#include "generated_api.h"

constexpr uint8_t kDeleteSlots = 4U;

struct DeleteState {
  mado::SlotTable<rest_delete::Item, kDeleteSlots> items;
};

template <typename ClientT, typename SdT, uint16_t kReadMode>
struct DeleteApp {
  using Exchange = mado::Exchange<ClientT, rest_delete::Limits>;

  static SdT*& card() {
    static SdT* instance = nullptr;
    return instance;
  }

  static DeleteState& state() {
    static DeleteState instance = {};
    return instance;
  }

  static void begin(SdT& sd, const uint8_t chip_select) {
    state() = DeleteState{};
    card() = sd.begin(chip_select) ? &sd : nullptr;
    seed("lamp", true);
    seed("fan", false);
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    const bool known = mado::check(route <= rest_delete::kCardFile);
    if (!known) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    return (route == rest_delete::kDeleteItem)
               ? delete_item(exchange, route)
               : rest_delete::send_card<kReadMode>(exchange, card(), route);
  }

 private:
  static void seed(const char* const name, const bool on) {
    rest_delete::Item item = {};
    strncpy(&item.name[0], name, sizeof(item.name) - 1U);
    item.on = on;
    const uint8_t slot = state().items.insert(item);
    rest_delete::Item* const stored = state().items.find(slot);
    const bool placed = mado::check(stored != nullptr);
    if (placed) {
      stored->id = slot;
    }
  }

  static bool delete_item(Exchange& exchange, const uint8_t route) {
    uint8_t slot = 0U;
    const bool parsed = rest_delete::route_path_id(exchange.path(), route, slot);
    const bool in_table = parsed && mado::check(slot > 0U) && (slot <= kDeleteSlots);
    const bool erased = in_table && state().items.erase(slot);
    if (!erased) {
      return mado::send_problem(exchange,
                                mado::Problem{mado::http::kNotFound, F("no item has this id")});
    }
    return exchange.send_empty(mado::http::kNoContent);
  }
};

#endif
