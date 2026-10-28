import { type Fetcher, sendCommand } from "@madoguchi/kit/api";

import type { ItemGateway } from "../../application/item.gateway.ts";

export function createHttpItemGateway(fetcher: Fetcher): ItemGateway {
  return {
    remove: (id) => sendCommand(fetcher, "DELETE", `/api/items/${id}`),
  };
}
