import { inputOf, say } from "@madoguchi/kit/page";

import type { ItemsView } from "../../application/items.view.ts";

export function createDomItemsView(document: Document): ItemsView {
  return {
    requestedId: () => inputOf(document, "item-id").valueAsNumber,
    showDeleted: (id) => {
      say(document, `Deleted item ${id}.`);
    },
  };
}
