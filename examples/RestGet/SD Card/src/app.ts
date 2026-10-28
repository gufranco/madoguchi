import { type Fetcher, readJson } from "@madoguchi/kit/api";
import { type Item, isItem, isItemList, itemLine } from "@madoguchi/kit/items";
import {
  type BoardHost,
  boardFetcher,
  inputOf,
  onSubmit,
  run,
  say,
  startPage,
} from "@madoguchi/kit/page";

function lineItem(document: Document, item: Item): HTMLLIElement {
  const entry = document.createElement("li");
  entry.append(itemLine(item));
  return entry;
}

async function listItems(document: Document, fetcher: Fetcher): Promise<void> {
  const items = await readJson(fetcher, "GET", "/api/items", isItemList);
  document
    .getElementById("items")
    ?.replaceChildren(...items.map((item) => lineItem(document, item)));
  say(document, items.length === 1 ? "1 item." : `${items.length} items.`);
}

async function findItem(document: Document, fetcher: Fetcher): Promise<void> {
  const id = inputOf(document, "item-id").valueAsNumber;
  const found = document.getElementById("found");
  found?.replaceChildren();
  const item = await readJson(fetcher, "GET", `/api/items/${id}`, isItem);
  found?.replaceChildren(itemLine(item));
  say(document, `Found item ${id}.`);
}

export function start(host: BoardHost): void {
  const fetcher = boardFetcher(host);
  startPage(host);
  onSubmit(host.document, "form", () => findItem(host.document, fetcher));
  run(host.document, () => listItems(host.document, fetcher));
}
