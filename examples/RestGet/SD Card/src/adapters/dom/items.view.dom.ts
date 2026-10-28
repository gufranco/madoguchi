import { inputOf, say } from "@madoguchi/kit/page";

import type { ItemsView } from "../../application/items.view.ts";
import { Item } from "../../domain/item.entity.ts";

function lineItem(document: Document, item: Item): HTMLLIElement {
  const entry = document.createElement("li");
  entry.append(Item.describe(item));
  return entry;
}

export function createDomItemsView(document: Document): ItemsView {
  return {
    requestedId: () => inputOf(document, "item-id").valueAsNumber,
    showList: (items) => {
      document
        .getElementById("items")
        ?.replaceChildren(...items.map((item) => lineItem(document, item)));
      say(document, items.length === 1 ? "1 item." : `${items.length} items.`);
    },
    clearFound: () => {
      document.getElementById("found")?.replaceChildren();
    },
    showFound: (item) => {
      document.getElementById("found")?.replaceChildren(Item.describe(item));
      say(document, `Found item ${item.id}.`);
    },
  };
}
