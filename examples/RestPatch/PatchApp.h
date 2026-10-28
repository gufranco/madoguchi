// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef PATCH_APP_H
#define PATCH_APP_H

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

constexpr uint8_t kPatchSlots = 4U;
constexpr uint32_t kPatchMaxBody = 256U;

struct PatchState {
  mado::SlotTable<rest_patch::Item, kPatchSlots> items;
};

template <typename ClientT, typename SdT, uint16_t kReadMode>
struct PatchApp {
  using Exchange = mado::Exchange<ClientT, rest_patch::Limits>;

  static SdT*& card() {
    static SdT* instance = nullptr;
    return instance;
  }

  static PatchState& state() {
    static PatchState instance = {};
    return instance;
  }

  static void begin(SdT& sd, const uint8_t chip_select) {
    state() = PatchState{};
    card() = sd.begin(chip_select) ? &sd : nullptr;
    seed("lamp", true);
    seed("fan", false);
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    const bool known = mado::check(route <= rest_patch::kCardFile);
    if (!known) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    return (route == rest_patch::kUpdateItem)
               ? update_item(exchange, route)
               : rest_patch::send_card<kReadMode>(exchange, card(), route);
  }

 private:
  static void seed(const char* const name, const bool on) {
    rest_patch::Item item = {};
    strncpy(&item.name[0], name, sizeof(item.name) - 1U);
    item.on = on;
    const uint8_t slot = state().items.insert(item);
    rest_patch::Item* const stored = state().items.find(slot);
    const bool placed = mado::check(stored != nullptr);
    if (placed) {
      stored->id = slot;
    }
  }

  static mado::Problem read_patch(Exchange& exchange, rest_patch::Item& item) {
    rest_patch::ItemPatch patch = {};
    uint8_t present = 0U;
    const mado::Problem problem =
        rest_patch::read_item_patch_body(exchange, patch, present, kPatchMaxBody);
    const bool sound = mado::check(present <= rest_patch::kItemPatchWritable);
    if ((problem.status != mado::http::kNoResponse) || (!sound)) {
      return problem;
    }
    if (present == 0U) {
      return mado::Problem{mado::http::kUnprocessableContent, F("the body changes no property")};
    }
    if ((present & rest_patch::kItemPatchName) != 0U) {
      memcpy(&item.name[0], &patch.name[0], sizeof(item.name));
    }
    item.on = ((present & rest_patch::kItemPatchOn) != 0U) ? patch.on : item.on;
    return problem;
  }

  static bool update_item(Exchange& exchange, const uint8_t route) {
    uint8_t slot = 0U;
    const bool parsed = rest_patch::route_path_id(exchange.path(), route, slot);
    const bool in_table = parsed && mado::check(slot > 0U) && (slot <= kPatchSlots);
    rest_patch::Item* const item = in_table ? state().items.find(slot) : nullptr;
    if (item == nullptr) {
      return mado::send_problem(exchange,
                                mado::Problem{mado::http::kNotFound, F("no item has this id")});
    }
    rest_patch::Item update = *item;
    const mado::Problem problem = read_patch(exchange, update);
    if (problem.status != mado::http::kNoResponse) {
      return mado::send_problem(exchange, problem);
    }
    *item = update;
    mado::Reply<Exchange> reply(exchange);
    return reply.status(mado::http::kOk).json().no_store().end() &&
           rest_patch::write_item(exchange, *item);
  }
};

#endif
