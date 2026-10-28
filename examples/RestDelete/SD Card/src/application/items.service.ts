import type { ItemGateway } from "./item.gateway.ts";
import type { ItemsView } from "./items.view.ts";

export function createItemsService(
  gateway: ItemGateway,
  view: ItemsView,
): { readonly removeRequested: () => Promise<void> } {
  return {
    async removeRequested(): Promise<void> {
      const id = view.requestedId();
      await gateway.remove(id);
      view.showDeleted(id);
    },
  } as const;
}
