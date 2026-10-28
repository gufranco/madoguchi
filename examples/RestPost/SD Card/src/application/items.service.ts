import type { ItemGateway } from "./item.gateway.ts";
import type { ItemsView } from "./items.view.ts";

export function createItemsService(
  gateway: ItemGateway,
  view: ItemsView,
): { readonly createRequested: () => Promise<void> } {
  return {
    async createRequested(): Promise<void> {
      view.showCreated(await gateway.create(view.draft()));
    },
  } as const;
}
