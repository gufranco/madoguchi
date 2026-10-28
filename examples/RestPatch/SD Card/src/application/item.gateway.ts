import type { Item } from "../domain/item.entity.ts";
import type { ItemPatch } from "../domain/item-patch.value.ts";

export interface ItemGateway {
  change(id: number, patch: ItemPatch): Promise<Item>;
}
