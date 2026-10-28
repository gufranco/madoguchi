import type { ItemGateway } from "./item.gateway.ts";
import type { ItemsView } from "./items.view.ts";

export function createItemsService(
  gateway: ItemGateway,
  view: ItemsView,
): { readonly replaceRequested: () => Promise<void> } {
  return {
    async replaceRequested(): Promise<void> {
      view.showReplaced(await gateway.replace(view.requestedId(), view.draft()));
    },
  } as const;
}
