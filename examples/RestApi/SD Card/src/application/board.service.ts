import type { Item } from "../domain/item.entity.ts";
import type { BoardGateway } from "./board.gateway.ts";
import type { BoardView } from "./board.view.ts";
import type { ItemActions } from "./item-actions.value.ts";
import type { TokenStore } from "./token.store.ts";

interface BoardService {
  readonly refresh: () => Promise<void>;
  readonly signIn: () => Promise<void>;
  readonly add: () => Promise<void>;
  readonly showLog: () => Promise<void>;
}

export function createBoardService(
  gateway: BoardGateway,
  view: BoardView,
  tokens: TokenStore,
): BoardService {
  const refresh = async (): Promise<void> => {
    view.showItems(await gateway.list(), actions, tokens.token() !== "");
  };
  const then = async (action: Promise<void>): Promise<void> => {
    await action;
    await refresh();
  };
  const actions: ItemActions = {
    toggle: (item: Item) => then(gateway.toggle(tokens.token(), item)),
    remove: (item: Item) => then(gateway.remove(tokens.token(), item)),
  };
  return {
    refresh,
    signIn: async () => {
      const credentials = view.credentials();
      view.clearCredentials();
      tokens.keep(await gateway.signIn(credentials));
      await refresh();
    },
    add: () => then(gateway.create(tokens.token(), view.draft())),
    showLog: async () => {
      view.showLog(await gateway.logTail(tokens.token()));
    },
  };
}
