import { type Fetcher, sendCommand } from "@madoguchi/kit/api";
import {
  type BoardHost,
  boardFetcher,
  inputOf,
  onSubmit,
  say,
  startPage,
} from "@madoguchi/kit/page";

async function deleteItem(document: Document, fetcher: Fetcher): Promise<void> {
  const id = inputOf(document, "item-id").valueAsNumber;
  await sendCommand(fetcher, "DELETE", `/api/items/${id}`);
  say(document, `Deleted item ${id}.`);
}

export function start(host: BoardHost): void {
  const fetcher = boardFetcher(host);
  startPage(host);
  onSubmit(host.document, "form", () => deleteItem(host.document, fetcher));
}
