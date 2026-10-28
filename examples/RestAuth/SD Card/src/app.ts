import { type Fetcher, readJson } from "@madoguchi/kit/api";
import { bearer, memorySession, type Session, signIn } from "@madoguchi/kit/auth";
import {
  type BoardHost,
  boardFetcher,
  inputOf,
  onSubmit,
  run,
  say,
  startPage,
} from "@madoguchi/kit/page";

interface Greeting {
  readonly message: string;
}

const isGreeting = (value: unknown): value is Greeting =>
  typeof value === "object" &&
  value !== null &&
  typeof (value as Partial<Greeting>).message === "string";

async function login(document: Document, fetcher: Fetcher, session: Session): Promise<void> {
  const credentials = {
    username: inputOf(document, "username").value,
    password: inputOf(document, "password").value,
  };
  document.querySelector<HTMLFormElement>("#login")?.reset();
  session.keep(await signIn(fetcher, credentials));
  say(document, "Signed in.");
}

async function greet(document: Document, fetcher: Fetcher, session: Session): Promise<void> {
  const greeting = await readJson(fetcher, "GET", "/api/greeting", isGreeting, {
    headers: bearer(session.token()),
  });
  document.getElementById("greeting")?.replaceChildren(greeting.message);
  say(document, "The board answered.");
}

export function start(host: BoardHost): void {
  const fetcher = boardFetcher(host);
  const session = memorySession();
  startPage(host);
  onSubmit(host.document, "#login", () => login(host.document, fetcher, session));
  host.document.getElementById("greet")?.addEventListener("click", () => {
    run(host.document, () => greet(host.document, fetcher, session));
  });
}
