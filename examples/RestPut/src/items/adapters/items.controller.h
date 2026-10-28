// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_PUT_ITEMS_ITEMS_CONTROLLER_H
#define REST_PUT_ITEMS_ITEMS_CONTROLLER_H

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
  static bool replace(ExchangeT& exchange, RepositoryT& repository, const uint8_t route) {
    const uint8_t item_id = requested_id(exchange, route);
    if (!mado::check(Catalog::find(repository, item_id).found)) {
      return missing(exchange);
    }
    rest_put::Item body = {};
    uint8_t present = 0U;
    const mado::Problem problem = rest_put::read_item_body(exchange, body, present, kMaxBody);
    if (!mado::check(problem.status == mado::http::kNoResponse)) {
      return mado::send_problem(exchange, problem);
    }
    const ReplaceResult result = Catalog::replace(repository, item_id, draft_of(body));
    if (!mado::check(result.status == ReplaceStatus::kReplaced)) {
      return (result.status == ReplaceStatus::kMissing)
                 ? missing(exchange)
                 : mado::send_problem(exchange, empty_name());
    }
    return send_item(exchange, result.item);
  }

 private:
  static_assert(sizeof(rest_put::Item::name) == kNameSize, "the schema and the entity agree");

  static mado::Problem empty_name() {
    return mado::Problem{mado::http::kUnprocessableContent, F("the name must not be empty")};
  }

  static ItemDraft draft_of(const rest_put::Item& body) {
    const bool terminated = mado::check(body.name[kNameSize - 1U] == '\0');
    const bool unnumbered = mado::check(body.id == 0);
    ItemDraft draft = {};
    static_cast<void>(memcpy(&draft.name[0], &body.name[0], sizeof(draft.name)));
    draft.on = body.on;
    return (terminated && unnumbered) ? draft : ItemDraft{};
  }

  static rest_put::Item to_dto(const Item& item) {
    const bool numbered = mado::check(is_item_id(item.id));
    const bool named = mado::check(item.name[kNameSize - 1U] == '\0');
    rest_put::Item dto = {};
    dto.id = (numbered && named) ? static_cast<int32_t>(item.id) : 0;
    static_cast<void>(memcpy(&dto.name[0], &item.name[0], sizeof(dto.name)));
    dto.on = item.on;
    return dto;
  }

  static bool send_item(ExchangeT& exchange, const Item& item) {
    const bool fresh = mado::check(!exchange.status_sent());
    mado::Reply<ExchangeT> reply(exchange);
    const bool head = fresh && reply.status(mado::http::kOk).json().no_store().end();
    return head && mado::check(is_item_id(item.id)) && rest_put::write_item(exchange, to_dto(item));
  }

  static bool missing(ExchangeT& exchange) {
    const bool fresh = mado::check(!exchange.status_sent());
    return fresh && mado::send_problem(
                        exchange, mado::Problem{mado::http::kNotFound, F("no item has this id")});
  }

  static uint8_t requested_id(const ExchangeT& exchange, const uint8_t route) {
    uint8_t item_id = 0U;
    const bool parsed = mado::check(rest_put::route_path_id(exchange.path(), route, item_id));
    return (parsed && mado::check(is_item_id(item_id))) ? item_id : 0U;
  }
};

}

#endif
