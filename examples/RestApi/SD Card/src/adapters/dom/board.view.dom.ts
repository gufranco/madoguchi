import { inputOf, run, say } from "@madoguchi/kit/page";

import type { BoardView } from "../../application/board.view.ts";
import type { ItemActions } from "../../application/item-actions.value.ts";
import { Item } from "../../domain/item.entity.ts";

interface RowAction {
  readonly name: "toggle" | "delete";
  readonly label: string;
  readonly run: () => Promise<void>;
}

function actionButton(document: Document, action: RowAction): HTMLButtonElement {
  const button = document.createElement("button");
  button.setAttribute("type", "button");
  button.setAttribute("data-action", action.name);
  button.append(action.label);
  button.addEventListener("click", () => run(document, action.run));
  return button;
}

function row(document: Document, item: Item, actions: ItemActions): HTMLLIElement {
  const entry = document.createElement("li");
  const text = document.createElement("span");
  text.append(Item.describe(item));
  entry.append(
    text,
    actionButton(document, {
      name: "toggle",
      label: `Toggle ${item.name}`,
      run: () => actions.toggle(item),
    }),
    actionButton(document, {
      name: "delete",
      label: `Delete ${item.name}`,
      run: () => actions.remove(item),
    }),
  );
  return entry;
}

export function createDomBoardView(document: Document): BoardView {
  return {
    credentials: () => ({
      username: inputOf(document, "username").value,
      password: inputOf(document, "password").value,
    }),
    clearCredentials: () => {
      document.querySelector<HTMLFormElement>("#login")?.reset();
    },
    draft: () => ({
      name: inputOf(document, "item-name").value,
      value: inputOf(document, "item-value").valueAsNumber,
      on: inputOf(document, "item-on").checked,
    }),
    showItems: (items, actions, signedIn) => {
      document
        .getElementById("items")
        ?.replaceChildren(...items.map((item) => row(document, item, actions)));
      say(document, signedIn ? "Done." : "Sign in to change items.");
    },
    showLog: (text) => {
      document.getElementById("log")?.replaceChildren(text);
    },
  };
}
