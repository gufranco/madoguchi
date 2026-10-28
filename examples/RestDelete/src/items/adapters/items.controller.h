// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_DELETE_ITEMS_ITEMS_CONTROLLER_H
#define REST_DELETE_ITEMS_ITEMS_CONTROLLER_H

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
  static bool remove(ExchangeT& exchange, RepositoryT& repository, const uint8_t route) {
    const uint8_t item_id = requested_id(exchange, route);
    const RemoveStatus status = Catalog::remove(repository, item_id);
    if (!mado::check(status == RemoveStatus::kRemoved)) {
      return missing(exchange);
    }
    return mado::check(!exchange.status_sent()) && exchange.send_empty(mado::http::kNoContent);
  }

 private:
  static bool missing(ExchangeT& exchange) {
    const bool fresh = mado::check(!exchange.status_sent());
    return fresh && mado::send_problem(
                        exchange, mado::Problem{mado::http::kNotFound, F("no item has this id")});
  }

  static uint8_t requested_id(const ExchangeT& exchange, const uint8_t route) {
    uint8_t item_id = 0U;
    const bool parsed = mado::check(rest_delete::route_path_id(exchange.path(), route, item_id));
    return (parsed && mado::check(is_item_id(item_id))) ? item_id : 0U;
  }
};

}

#endif
