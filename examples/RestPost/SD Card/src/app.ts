import { type Fetcher, readJson } from "@madoguchi/kit/api";
import { isItem, itemLine } from "@madoguchi/kit/items";
import {
  type BoardHost,
  boardFetcher,
  inputOf,
  onSubmit,
  say,
  startPage,
} from "@madoguchi/kit/page";

async function createItem(document: Document, fetcher: Fetcher): Promise<void> {
  const body = {
    name: inputOf(document, "item-name").value,
    on: inputOf(document, "item-on").checked,
  };
  const item = await readJson(fetcher, "POST", "/api/items", isItem, {
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  document.getElementById("result")?.replaceChildren(itemLine(item));
  say(document, `Created item ${item.id}.`);
}

export function start(host: BoardHost): void {
  const fetcher = boardFetcher(host);
  startPage(host);
  onSubmit(host.document, "form", () => createItem(host.document, fetcher));
}
