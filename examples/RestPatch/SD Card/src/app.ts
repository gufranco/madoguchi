import { type Fetcher, readJson } from "@madoguchi/kit/api";
import { isItem, itemLine } from "@madoguchi/kit/items";
import {
  type BoardHost,
  boardFetcher,
  inputOf,
  onSubmit,
  say,
  selectOf,
  startPage,
} from "@madoguchi/kit/page";

export interface PatchFields {
  readonly name: string;
  readonly state: string;
}

export interface ItemPatch {
  readonly name?: string;
  readonly on?: boolean;
}

export function patchOf(fields: PatchFields): ItemPatch {
  const named = fields.name === "" ? {} : { name: fields.name };
  const switched = fields.state === "keep" ? {} : { on: fields.state === "on" };
  return { ...named, ...switched };
}

async function patchItem(document: Document, fetcher: Fetcher): Promise<void> {
  const id = inputOf(document, "item-id").valueAsNumber;
  const patch = patchOf({
    name: inputOf(document, "item-name").value,
    state: selectOf(document, "item-state").value,
  });
  if (Object.keys(patch).length === 0) {
    say(document, "Fill in a new name or pick a switch state.");
    return;
  }
  const item = await readJson(fetcher, "PATCH", `/api/items/${id}`, isItem, {
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(patch),
  });
  document.getElementById("result")?.replaceChildren(itemLine(item));
  say(document, `Changed item ${item.id}.`);
}

export function start(host: BoardHost): void {
  const fetcher = boardFetcher(host);
  startPage(host);
  onSubmit(host.document, "form", () => patchItem(host.document, fetcher));
}
