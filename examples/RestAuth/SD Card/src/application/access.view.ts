import type { Credentials } from "../domain/credentials.value.ts";
import type { Greeting } from "../domain/greeting.entity.ts";

export interface AccessView {
  credentials(): Credentials;
  clearCredentials(): void;
  showSignedIn(): void;
  showGreeting(greeting: Greeting): void;
}
