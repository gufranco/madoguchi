// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_POST_ITEMS_ITEMS_CONTROLLER_H
#define REST_POST_ITEMS_ITEMS_CONTROLLER_H

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
  static bool create(ExchangeT& exchange, RepositoryT& repository) {
    rest_post::Item body = {};
    uint8_t present = 0U;
    const mado::Problem problem = rest_post::read_item_body(exchange, body, present, kMaxBody);
    if (!mado::check(problem.status == mado::http::kNoResponse)) {
      return mado::send_problem(exchange, problem);
    }
    const AddResult result = Catalog::add(repository, draft_of(body));
    if (!mado::check(result.status == AddStatus::kAdded)) {
      return mado::send_problem(exchange, refusal(result.status));
    }
    mado::Reply<ExchangeT> reply(exchange);
    const bool head = reply.status(mado::http::kCreated)
                          .json()
                          .no_store()
                          .location(F("/api/items/"), result.item.id)
                          .end();
    return head && rest_post::write_item(exchange, to_dto(result.item));
  }

 private:
  static_assert(sizeof(rest_post::Item::name) == kNameSize, "the schema and the entity agree");

  static ItemDraft draft_of(const rest_post::Item& body) {
    const bool terminated = mado::check(body.name[kNameSize - 1U] == '\0');
    const bool unnumbered = mado::check(body.id == 0);
    ItemDraft draft = {};
    static_cast<void>(memcpy(&draft.name[0], &body.name[0], sizeof(draft.name)));
    draft.on = body.on;
    return (terminated && unnumbered) ? draft : ItemDraft{};
  }

  static mado::Problem refusal(const AddStatus status) {
    const bool refused = mado::check(status != AddStatus::kAdded);
    if (refused && mado::check(status == AddStatus::kFull)) {
      return mado::Problem{mado::http::kInsufficientStorage, F("every item slot is taken")};
    }
    return mado::Problem{mado::http::kUnprocessableContent, F("the name must not be empty")};
  }

  static rest_post::Item to_dto(const Item& item) {
    const bool numbered = mado::check(is_item_id(item.id));
    const bool named = mado::check(item.name[kNameSize - 1U] == '\0');
    rest_post::Item dto = {};
    dto.id = (numbered && named) ? static_cast<int32_t>(item.id) : 0;
    static_cast<void>(memcpy(&dto.name[0], &item.name[0], sizeof(dto.name)));
    dto.on = item.on;
    return dto;
  }
};

}

#endif
