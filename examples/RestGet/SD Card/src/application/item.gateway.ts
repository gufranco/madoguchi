import type { Item } from "../domain/item.entity.ts";

export interface ItemGateway {
  list(): Promise<readonly Item[]>;
  find(id: number): Promise<Item>;
}
