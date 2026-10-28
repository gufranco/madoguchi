import { type Fetcher, readJson } from "@madoguchi/kit/api";

import type { ItemGateway } from "../../application/item.gateway.ts";
import { Item } from "../../domain/item.entity.ts";

export function createHttpItemGateway(fetcher: Fetcher): ItemGateway {
  return {
    create: (draft) =>
      readJson(fetcher, "POST", "/api/items", Item.isOne, {
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(draft),
      }),
  };
}
