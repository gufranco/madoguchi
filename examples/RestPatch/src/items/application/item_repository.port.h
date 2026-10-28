// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_PATCH_ITEMS_ITEM_REPOSITORY_PORT_H
#define REST_PATCH_ITEMS_ITEM_REPOSITORY_PORT_H

#include <mado/reference_to.h>
#include <mado/same_type.h>
#include <stdint.h>

#include "../domain/item.entity.h"

namespace items {

template <typename RepositoryT>
struct ItemRepositoryPort {
  using Insert =
      decltype(mado::reference_to<RepositoryT>().insert(mado::reference_to<const ItemDraft>()));
  using Find =
      decltype(mado::reference_to<const RepositoryT>().find(uint8_t{}, mado::reference_to<Item>()));
  using Store = decltype(mado::reference_to<RepositoryT>().store(mado::reference_to<const Item>()));

  static_assert(mado::same_type<Insert, uint8_t>::value,
                "insert(const ItemDraft&) stores the item and returns its id, or 0 when full");
  static_assert(mado::same_type<Find, bool>::value,
                "find(id, Item&) copies the item out and returns whether the id holds one");
  static_assert(
      mado::same_type<Store, bool>::value,
      "store(const Item&) overwrites the item under its id and returns whether it held one");

  static constexpr bool kSatisfied = true;
};

}

#endif
