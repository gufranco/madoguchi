import { type Fetcher, readJson, readText, sendCommand } from "@madoguchi/kit/api";
import { bearer, signIn } from "@madoguchi/kit/auth";

import type { BoardGateway } from "../../application/board.gateway.ts";
import { Item } from "../../domain/item.entity.ts";

const LOG_TAIL = "bytes=-4096";

function jsonInit(token: string, body: unknown): RequestInit {
  return {
    headers: { "Content-Type": "application/json", ...bearer(token) },
    body: JSON.stringify(body),
  };
}

export function createHttpBoardGateway(fetcher: Fetcher): BoardGateway {
  return {
    signIn: (credentials) => signIn(fetcher, credentials),
    list: () => readJson(fetcher, "GET", "/api/items", Item.isList),
    create: async (token, draft) => {
      await readJson(fetcher, "POST", "/api/items", Item.isOne, jsonInit(token, draft));
    },
    toggle: async (token, item) => {
      await readJson(
        fetcher,
        "PATCH",
        `/api/items/${item.id}`,
        Item.isOne,
        jsonInit(token, { on: !item.on }),
      );
    },
    remove: (token, item) =>
      sendCommand(fetcher, "DELETE", `/api/items/${item.id}`, { headers: bearer(token) }),
    logTail: (token) =>
      readText(fetcher, "GET", "/api/log", { headers: { Range: LOG_TAIL, ...bearer(token) } }),
  };
}
