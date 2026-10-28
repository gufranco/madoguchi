import type { Credentials } from "../domain/credentials.value.ts";
import type { Greeting } from "../domain/greeting.entity.ts";

export interface AccessGateway {
  signIn(credentials: Credentials): Promise<string>;
  greeting(token: string): Promise<Greeting>;
}
