import { type Fetcher, readJson, readText, sendCommand } from "@madoguchi/kit/api";
import { bearer, memorySession, type Session, signIn } from "@madoguchi/kit/auth";
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

const LOG_TAIL = "bytes=-4096";

interface Context {
  readonly document: Document;
  readonly fetcher: Fetcher;
  readonly session: Session;
}

function jsonInit(context: Context, body: unknown): RequestInit {
  return {
    headers: { "Content-Type": "application/json", ...bearer(context.session.token()) },
    body: JSON.stringify(body),
  };
}

interface RowAction {
  readonly name: "toggle" | "delete";
  readonly label: string;
  readonly run: () => Promise<void>;
}

function actionButton(context: Context, action: RowAction): HTMLButtonElement {
  const button = context.document.createElement("button");
  button.setAttribute("type", "button");
  button.setAttribute("data-action", action.name);
  button.append(action.label);
  button.addEventListener("click", () => run(context.document, () => changed(context, action.run)));
  return button;
}

function row(context: Context, item: Item): HTMLLIElement {
  const entry = context.document.createElement("li");
  const text = context.document.createElement("span");
  text.append(itemLine(item));
  const toggle = () =>
    readJson(
      context.fetcher,
      "PATCH",
      `/api/items/${item.id}`,
      isItem,
      jsonInit(context, { on: !item.on }),
    ).then(() => undefined);
  const remove = () =>
    sendCommand(context.fetcher, "DELETE", `/api/items/${item.id}`, {
      headers: bearer(context.session.token()),
    });
  entry.append(
    text,
    actionButton(context, { name: "toggle", label: `Toggle ${item.name}`, run: toggle }),
    actionButton(context, { name: "delete", label: `Delete ${item.name}`, run: remove }),
  );
  return entry;
}

async function refresh(context: Context): Promise<void> {
  const items = await readJson(context.fetcher, "GET", "/api/items", isItemList);
  context.document
    .getElementById("items")
    ?.replaceChildren(...items.map((item) => row(context, item)));
  say(context.document, context.session.token() === "" ? "Sign in to change items." : "Done.");
}

async function changed(context: Context, action: () => Promise<void>): Promise<void> {
  await action();
  await refresh(context);
}

async function login(context: Context): Promise<void> {
  const credentials = {
    username: inputOf(context.document, "username").value,
    password: inputOf(context.document, "password").value,
  };
  context.document.querySelector<HTMLFormElement>("#login")?.reset();
  context.session.keep(await signIn(context.fetcher, credentials));
}

async function addItem(context: Context): Promise<void> {
  const body = {
    name: inputOf(context.document, "item-name").value,
    value: inputOf(context.document, "item-value").valueAsNumber,
    on: inputOf(context.document, "item-on").checked,
  };
  await readJson(context.fetcher, "POST", "/api/items", isItem, jsonInit(context, body));
}

async function showLog(context: Context): Promise<void> {
  const text = await readText(context.fetcher, "GET", "/api/log", {
    headers: { Range: LOG_TAIL, ...bearer(context.session.token()) },
  });
  context.document.getElementById("log")?.replaceChildren(text);
}

export function start(host: BoardHost): void {
  const context: Context = {
    document: host.document,
    fetcher: boardFetcher(host),
    session: memorySession(),
  };
  startPage(host);
  onSubmit(host.document, "#login", () => changed(context, () => login(context)));
  onSubmit(host.document, "#add", () => changed(context, () => addItem(context)));
  host.document.getElementById("show-log")?.addEventListener("click", () => {
    run(host.document, () => showLog(context));
  });
  run(host.document, () => refresh(context));
}
