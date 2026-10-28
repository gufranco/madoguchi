import { type Fetcher, readJson } from "@madoguchi/kit/api";

import type { ItemGateway } from "../../application/item.gateway.ts";
import { Item } from "../../domain/item.entity.ts";

export function createHttpItemGateway(fetcher: Fetcher): ItemGateway {
  return {
    list: () => readJson(fetcher, "GET", "/api/items", Item.isList),
    find: (id) => readJson(fetcher, "GET", `/api/items/${id}`, Item.isOne),
  };
}
