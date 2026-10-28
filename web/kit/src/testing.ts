import { Window } from "happy-dom";

import type { Fetcher } from "./api.ts";
import type { BoardHost } from "./page.ts";

export interface Call {
  readonly path: string;
  readonly method: string;
  readonly body: BodyInit | null | undefined;
  readonly headers: Readonly<Record<string, string>>;
}

export interface FakeBoard {
  readonly host: BoardHost;
  readonly window: Window;
  readonly calls: () => readonly Call[];
  readonly text: (selector: string) => string;
  readonly input: (id: string) => HTMLInputElement;
  readonly submit: (selector: string) => void;
  readonly click: (selector: string) => void;
}

export function answer(status: number, body: unknown): Response {
  const text = status === 204 ? null : JSON.stringify(body);
  return new Response(text, { status, headers: { "Content-Type": "application/json" } });
}

export function problem(status: number, detail: string): Response {
  return new Response(JSON.stringify({ title: "Error", status, detail }), {
    status,
    headers: { "Content-Type": "application/problem+json" },
  });
}

function headersOf(init: RequestInit): Readonly<Record<string, string>> {
  const pairs = [...new Headers(init.headers).entries()];
  return Object.fromEntries(pairs.map(([name, value]) => [name.toLowerCase(), value]));
}

export function fakeBoard(page: string, answers: readonly Response[]): FakeBoard {
  const window = new Window({ url: "http://board.test/" });
  window.document.write(page);
  let calls: readonly Call[] = [];
  let queue = [...answers];
  const fetch: Fetcher = async (path, init) => {
    const call = { path, method: init.method ?? "GET", body: init.body, headers: headersOf(init) };
    calls = [...calls, call];
    const [next, ...rest] = queue;
    queue = rest;
    return next ?? answer(500, {});
  };
  const host: BoardHost = {
    document: window.document as unknown as Document,
    localStorage: window.localStorage as unknown as Storage,
    matchMedia: (query) => window.matchMedia(query) as unknown as MediaQueryList,
    fetch,
  };
  const input = (id: string): HTMLInputElement => {
    const found = window.document.querySelector(`#${id}`);
    if (found === null) {
      throw new Error(`no #${id}`);
    }
    return found as unknown as HTMLInputElement;
  };
  return {
    host,
    window,
    calls: () => calls,
    text: (selector) => window.document.querySelector(selector)?.textContent ?? "",
    input,
    click: (selector) => {
      const target = window.document.querySelector(selector) as unknown as HTMLElement | null;
      target?.click();
    },
    submit: (selector) => {
      const form = window.document.querySelector(selector) as unknown as HTMLFormElement | null;
      form?.requestSubmit();
    },
  };
}

export const TEST_TOKEN = "eyJ0ZXN0IjoidG9rZW4ifQ.e30.c2lnbmF0dXJl";

export function tokenAnswer(): Response {
  return answer(200, { access_token: TEST_TOKEN, token_type: "Bearer", expires_in: 900 });
}

export function signInOn(board: FakeBoard): void {
  board.input("username").value = "admin";
  board.input("password").value = "correct-horse-battery";
  board.submit("#login");
}
