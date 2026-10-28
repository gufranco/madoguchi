// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_PATCH_ITEMS_ITEMS_CONTROLLER_H
#define REST_PATCH_ITEMS_ITEMS_CONTROLLER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>
#include <string.h>

#include "../../http/api.generated.h"
#include "../application/item_catalog.service.h"
#include "../domain/item.entity.h"

namespace items {

constexpr uint32_t kMaxBody = 256U;

template <typename ExchangeT, typename RepositoryT>
class ItemsController {
  using Catalog = ItemCatalog<RepositoryT>;

 public:
  static bool update(ExchangeT& exchange, RepositoryT& repository, const uint8_t route) {
    const uint8_t item_id = requested_id(exchange, route);
    if (!mado::check(Catalog::find(repository, item_id).found)) {
      return missing(exchange);
    }
    rest_patch::ItemPatch body = {};
    uint8_t present = 0U;
    const mado::Problem problem =
        rest_patch::read_item_patch_body(exchange, body, present, kMaxBody);
    if (!mado::check(problem.status == mado::http::kNoResponse)) {
      return mado::send_problem(exchange, problem);
    }
    const ChangeResult result = Catalog::change(repository, item_id, change_of(body, present));
    if (!mado::check(result.status == ChangeOutcome::kChanged)) {
      return (result.status == ChangeOutcome::kMissing)
                 ? missing(exchange)
                 : mado::send_problem(exchange, refusal(result.status));
    }
    return send_item(exchange, result.item);
  }

 private:
  static_assert(sizeof(rest_patch::ItemPatch::name) == kNameSize,
                "the schema and the entity agree");

  static ItemChange change_of(const rest_patch::ItemPatch& body, const uint8_t present) {
    const bool sound =
        mado::check((present & static_cast<uint8_t>(~rest_patch::kItemPatchWritable)) == 0U);
    const bool terminated = mado::check(body.name[kNameSize - 1U] == '\0');
    ItemChange change = {};
    change.renames = sound && terminated && ((present & rest_patch::kItemPatchName) != 0U);
    static_cast<void>(memcpy(&change.name[0], &body.name[0], sizeof(change.name)));
    change.switches = sound && ((present & rest_patch::kItemPatchOn) != 0U);
    change.on = body.on;
    return change;
  }

  static mado::Problem refusal(const ChangeOutcome status) {
    const bool refused = mado::check(status != ChangeOutcome::kChanged);
    if (refused && mado::check(status == ChangeOutcome::kNothing)) {
      return mado::Problem{mado::http::kUnprocessableContent, F("the body changes no property")};
    }
    return mado::Problem{mado::http::kUnprocessableContent, F("the name must not be empty")};
  }

  static rest_patch::Item to_dto(const Item& item) {
    const bool numbered = mado::check(is_item_id(item.id));
    const bool named = mado::check(item.name[kNameSize - 1U] == '\0');
    rest_patch::Item dto = {};
    dto.id = (numbered && named) ? static_cast<int32_t>(item.id) : 0;
    static_cast<void>(memcpy(&dto.name[0], &item.name[0], sizeof(dto.name)));
    dto.on = item.on;
    return dto;
  }

  static bool send_item(ExchangeT& exchange, const Item& item) {
    const bool fresh = mado::check(!exchange.status_sent());
    mado::Reply<ExchangeT> reply(exchange);
    const bool head = fresh && reply.status(mado::http::kOk).json().no_store().end();
    return head && mado::check(is_item_id(item.id)) &&
           rest_patch::write_item(exchange, to_dto(item));
  }

  static bool missing(ExchangeT& exchange) {
    const bool fresh = mado::check(!exchange.status_sent());
    return fresh && mado::send_problem(
                        exchange, mado::Problem{mado::http::kNotFound, F("no item has this id")});
  }

  static uint8_t requested_id(const ExchangeT& exchange, const uint8_t route) {
    uint8_t item_id = 0U;
    const bool parsed = mado::check(rest_patch::route_path_id(exchange.path(), route, item_id));
    return (parsed && mado::check(is_item_id(item_id))) ? item_id : 0U;
  }
};

}

#endif
