import type { Credentials } from "../domain/credentials.value.ts";
import type { Item } from "../domain/item.entity.ts";
import type { ItemDraft } from "../domain/item-draft.value.ts";

export interface BoardGateway {
  signIn(credentials: Credentials): Promise<string>;
  list(): Promise<readonly Item[]>;
  create(token: string, draft: ItemDraft): Promise<void>;
  toggle(token: string, item: Item): Promise<void>;
  remove(token: string, item: Item): Promise<void>;
  logTail(token: string): Promise<string>;
}
