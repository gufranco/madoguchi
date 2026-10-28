// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_GET_ITEMS_ITEMS_CONTROLLER_H
#define REST_GET_ITEMS_ITEMS_CONTROLLER_H

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

template <typename ExchangeT, typename RepositoryT>
class ItemsController {
  using Catalog = ItemCatalog<RepositoryT>;

 public:
  static bool list(ExchangeT& exchange, const RepositoryT& repository) {
    bool written = send_json_head(exchange, mado::http::kOk) && (exchange.print(F("[")) > 0U);
    bool first = true;
    for (uint8_t item_id = 1U; written && (item_id <= kCapacity); ++item_id) {
      const ItemLookup lookup = Catalog::find(repository, item_id);
      if (!lookup.found) {
        continue;
      }
      const bool separated = first || (exchange.print(F(",")) > 0U);
      written = separated && mado::check(lookup.item.id == item_id) &&
                rest_get::write_item(exchange, to_dto(lookup.item));
      first = false;
    }
    return written && (exchange.print(F("]")) > 0U);
  }

  static bool get(ExchangeT& exchange, const RepositoryT& repository, const uint8_t route) {
    uint8_t item_id = 0U;
    const bool parsed = mado::check(rest_get::route_path_id(exchange.path(), route, item_id));
    const ItemLookup lookup =
        parsed ? Catalog::find(repository, item_id) : ItemLookup{false, Item{}};
    if (!mado::check(lookup.found)) {
      return mado::send_problem(exchange,
                                mado::Problem{mado::http::kNotFound, F("no item has this id")});
    }
    return send_json_head(exchange, mado::http::kOk) &&
           rest_get::write_item(exchange, to_dto(lookup.item));
  }

 private:
  static_assert(sizeof(rest_get::Item::name) == kNameSize, "the schema and the entity agree");

  static rest_get::Item to_dto(const Item& item) {
    const bool numbered = mado::check(is_item_id(item.id));
    const bool named = mado::check(item.name[kNameSize - 1U] == '\0');
    rest_get::Item dto = {};
    dto.id = (numbered && named) ? static_cast<int32_t>(item.id) : 0;
    static_cast<void>(memcpy(&dto.name[0], &item.name[0], sizeof(dto.name)));
    dto.on = item.on;
    return dto;
  }

  static bool send_json_head(ExchangeT& exchange, const uint16_t code) {
    const bool fresh = mado::check(!exchange.status_sent());
    const bool success = mado::check(code == mado::http::kOk);
    mado::Reply<ExchangeT> reply(exchange);
    return fresh && success && reply.status(code).json().no_store().end();
  }
};

}

#endif
