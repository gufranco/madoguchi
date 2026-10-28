// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SAME_TYPE_H
#define MADO_SAME_TYPE_H

namespace mado {

template <typename Left, typename Right>
struct same_type {
  static constexpr bool value = false;
};

template <typename Both>
struct same_type<Both, Both> {
  static constexpr bool value = true;
};

}

#endif
