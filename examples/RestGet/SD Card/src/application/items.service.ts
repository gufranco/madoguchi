import type { ItemGateway } from "./item.gateway.ts";
import type { ItemsView } from "./items.view.ts";

export function createItemsService(
  gateway: ItemGateway,
  view: ItemsView,
): { readonly load: () => Promise<void>; readonly findRequested: () => Promise<void> } {
  return {
    async load(): Promise<void> {
      view.showList(await gateway.list());
    },
    async findRequested(): Promise<void> {
      const id = view.requestedId();
      view.clearFound();
      view.showFound(await gateway.find(id));
    },
  } as const;
}
