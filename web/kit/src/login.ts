import type { Fetcher } from "./api.ts";
import { bearer, type Credentials, type Session, signIn } from "./auth.ts";
import { type BoardHost, boardFetcher, inputOf, onSubmit, say } from "./page.ts";

export const SIGN_IN_FIRST = "Sign in to use this page.";

export function authorizedFetcher(fetcher: Fetcher, session: Session): Fetcher {
  return (path, init) => {
    const headers = {
      ...Object.fromEntries(new Headers(init.headers).entries()),
      ...bearer(session.token()),
    };
    return fetcher(path, { ...init, headers });
  };
}

export function credentialsOf(document: Document): Credentials {
  return {
    username: inputOf(document, "username").value,
    password: inputOf(document, "password").value,
  };
}

export function onSignIn(host: BoardHost, session: Session, signedIn: () => Promise<void>): void {
  say(host.document, SIGN_IN_FIRST);
  onSubmit(host.document, "#login", async () => {
    session.keep(await signIn(boardFetcher(host), credentialsOf(host.document)));
    say(host.document, "Signed in.");
    await signedIn();
  });
}
