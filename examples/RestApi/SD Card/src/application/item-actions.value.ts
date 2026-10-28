import type { Item } from "../domain/item.entity.ts";

export interface ItemActions {
  readonly toggle: (item: Item) => Promise<void>;
  readonly remove: (item: Item) => Promise<void>;
}
