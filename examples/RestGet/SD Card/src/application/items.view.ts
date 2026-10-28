import type { Item } from "../domain/item.entity.ts";

export interface ItemsView {
  requestedId(): number;
  showList(items: readonly Item[]): void;
  clearFound(): void;
  showFound(item: Item): void;
}
