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

async function replaceItem(document: Document, fetcher: Fetcher): Promise<void> {
  const id = inputOf(document, "item-id").valueAsNumber;
  const body = {
    name: inputOf(document, "item-name").value,
    on: inputOf(document, "item-on").checked,
  };
  const item = await readJson(fetcher, "PUT", `/api/items/${id}`, isItem, {
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  document.getElementById("result")?.replaceChildren(itemLine(item));
  say(document, `Replaced item ${item.id}.`);
}

export function start(host: BoardHost): void {
  const fetcher = boardFetcher(host);
  startPage(host);
  onSubmit(host.document, "form", () => replaceItem(host.document, fetcher));
}
