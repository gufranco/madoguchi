import type { Item } from "../domain/item.entity.ts";
import type { ItemDraft } from "../domain/item-draft.value.ts";

export interface ItemGateway {
  replace(id: number, draft: ItemDraft): Promise<Item>;
}
