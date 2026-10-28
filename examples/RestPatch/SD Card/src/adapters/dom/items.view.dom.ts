import { inputOf, say, selectOf } from "@madoguchi/kit/page";

import type { ItemsView } from "../../application/items.view.ts";
import { Item } from "../../domain/item.entity.ts";

export function createDomItemsView(document: Document): ItemsView {
  return {
    requestedId: () => inputOf(document, "item-id").valueAsNumber,
    fields: () => ({
      name: inputOf(document, "item-name").value,
      state: selectOf(document, "item-state").value,
    }),
    askForAChange: () => {
      say(document, "Fill in a new name or pick a switch state.");
    },
    showChanged: (item) => {
      document.getElementById("result")?.replaceChildren(Item.describe(item));
      say(document, `Changed item ${item.id}.`);
    },
  };
}
