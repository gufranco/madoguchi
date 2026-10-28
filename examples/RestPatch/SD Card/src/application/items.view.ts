import type { Item } from "../domain/item.entity.ts";
import type { PatchFields } from "../domain/item-patch.value.ts";

export interface ItemsView {
  requestedId(): number;
  fields(): PatchFields;
  askForAChange(): void;
  showChanged(item: Item): void;
}
