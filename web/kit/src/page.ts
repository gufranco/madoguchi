import type { Fetcher } from "./api.ts";
import { mountTheme } from "./theme.ts";

export type BoardHost = Pick<Window, "document" | "localStorage" | "matchMedia"> & {
  readonly fetch: Fetcher;
};

export function safeStorage(host: Pick<Window, "localStorage">): Storage | undefined {
  try {
    return host.localStorage;
  } catch {
    return undefined;
  }
}

export function startPage(host: Pick<Window, "document" | "localStorage" | "matchMedia">): void {
  mountTheme({
    document: host.document,
    storage: safeStorage(host),
    match: (query) => host.matchMedia(query),
  });
}

export function say(document: Document, message: string): void {
  document.querySelector('[role="status"]')?.replaceChildren(message);
}

export function report(document: Document, error: unknown): void {
  say(document, error instanceof Error ? error.message : String(error));
}

export function element<T extends Element>(
  document: Document,
  id: string,
  kind: abstract new () => T,
): T {
  const found = document.getElementById(id);
  if (!(found instanceof kind)) {
    throw new Error(`the page has no #${id}`);
  }
  return found;
}

export function inputOf(document: Document, id: string): HTMLInputElement {
  const view = document.defaultView;
  if (view === null) {
    throw new Error(`the page has no #${id}`);
  }
  return element(document, id, view.HTMLInputElement);
}

export function boardFetcher(host: { readonly fetch: Fetcher }): Fetcher {
  return (path, init) => host.fetch(path, init);
}

export function run(document: Document, action: () => Promise<void>): void {
  action().catch((error: unknown) => report(document, error));
}

export function onSubmit(document: Document, selector: string, action: () => Promise<void>): void {
  document.querySelector(selector)?.addEventListener("submit", (event) => {
    event.preventDefault();
    run(document, action);
  });
}

export function selectOf(document: Document, id: string): HTMLSelectElement {
  const view = document.defaultView;
  if (view === null) {
    throw new Error(`the page has no #${id}`);
  }
  return element(document, id, view.HTMLSelectElement);
}
