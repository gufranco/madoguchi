import { inputOf, say } from "@madoguchi/kit/page";

import type { ItemsView } from "../../application/items.view.ts";
import { Item } from "../../domain/item.entity.ts";

export function createDomItemsView(document: Document): ItemsView {
  return {
    draft: () => ({
      name: inputOf(document, "item-name").value,
      on: inputOf(document, "item-on").checked,
    }),
    showCreated: (item) => {
      document.getElementById("result")?.replaceChildren(Item.describe(item));
      say(document, `Created item ${item.id}.`);
    },
  };
}
