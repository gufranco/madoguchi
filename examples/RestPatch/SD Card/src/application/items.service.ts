import { ItemPatch } from "../domain/item-patch.value.ts";
import type { ItemGateway } from "./item.gateway.ts";
import type { ItemsView } from "./items.view.ts";

export function createItemsService(
  gateway: ItemGateway,
  view: ItemsView,
): { readonly changeRequested: () => Promise<void> } {
  return {
    async changeRequested(): Promise<void> {
      const patch = ItemPatch.from(view.fields());
      if (ItemPatch.changesNothing(patch)) {
        view.askForAChange();
        return;
      }
      view.showChanged(await gateway.change(view.requestedId(), patch));
    },
  } as const;
}
