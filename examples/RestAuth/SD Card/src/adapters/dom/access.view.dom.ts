import { inputOf, say } from "@madoguchi/kit/page";

import type { AccessView } from "../../application/access.view.ts";

export function createDomAccessView(document: Document): AccessView {
  return {
    credentials: () => ({
      username: inputOf(document, "username").value,
      password: inputOf(document, "password").value,
    }),
    clearCredentials: () => {
      document.querySelector<HTMLFormElement>("#login")?.reset();
    },
    showSignedIn: () => {
      say(document, "Signed in.");
    },
    showGreeting: (greeting) => {
      document.getElementById("greeting")?.replaceChildren(greeting.message);
      say(document, "The board answered.");
    },
  };
}
