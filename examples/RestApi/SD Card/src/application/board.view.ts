import type { Credentials } from "../domain/credentials.value.ts";
import type { Item } from "../domain/item.entity.ts";
import type { ItemDraft } from "../domain/item-draft.value.ts";
import type { ItemActions } from "./item-actions.value.ts";

export interface BoardView {
  credentials(): Credentials;
  clearCredentials(): void;
  draft(): ItemDraft;
  showItems(items: readonly Item[], actions: ItemActions, signedIn: boolean): void;
  showLog(text: string): void;
}
