import { type Fetcher, readJson } from "@madoguchi/kit/api";

import type { ItemGateway } from "../../application/item.gateway.ts";
import { Item } from "../../domain/item.entity.ts";

export function createHttpItemGateway(fetcher: Fetcher): ItemGateway {
  return {
    replace: (id, draft) =>
      readJson(fetcher, "PUT", `/api/items/${id}`, Item.isOne, {
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(draft),
      }),
  };
}
