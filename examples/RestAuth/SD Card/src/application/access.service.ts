import type { AccessGateway } from "./access.gateway.ts";
import type { AccessView } from "./access.view.ts";
import type { TokenStore } from "./token.store.ts";

export function createAccessService(
  gateway: AccessGateway,
  view: AccessView,
  tokens: TokenStore,
): { readonly signIn: () => Promise<void>; readonly greet: () => Promise<void> } {
  return {
    async signIn(): Promise<void> {
      const credentials = view.credentials();
      view.clearCredentials();
      tokens.keep(await gateway.signIn(credentials));
      view.showSignedIn();
    },
    async greet(): Promise<void> {
      view.showGreeting(await gateway.greeting(tokens.token()));
    },
  } as const;
}
