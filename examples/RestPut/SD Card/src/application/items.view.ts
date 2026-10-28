import type { Item } from "../domain/item.entity.ts";
import type { ItemDraft } from "../domain/item-draft.value.ts";

export interface ItemsView {
  requestedId(): number;
  draft(): ItemDraft;
  showReplaced(item: Item): void;
}
