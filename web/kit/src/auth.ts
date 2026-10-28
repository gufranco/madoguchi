import { type Fetcher, readJson } from "./api.ts";

export interface Credentials {
  readonly username: string;
  readonly password: string;
}

export interface Token {
  readonly access_token: string;
  readonly token_type: "Bearer";
  readonly expires_in: number;
}

export function isToken(value: unknown): value is Token {
  if (typeof value !== "object" || value === null) {
    return false;
  }
  const fields = value as Readonly<Record<string, unknown>>;
  return (
    typeof fields["access_token"] === "string" &&
    fields["token_type"] === "Bearer" &&
    typeof fields["expires_in"] === "number"
  );
}

export async function signIn(fetcher: Fetcher, credentials: Credentials): Promise<string> {
  const token = await readJson(fetcher, "POST", "/api/auth/login", isToken, {
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(credentials),
  });
  return token.access_token;
}

export function bearer(token: string): Readonly<Record<string, string>> {
  return token === "" ? {} : { Authorization: `Bearer ${token}` };
}

export interface Session {
  readonly token: () => string;
  readonly keep: (token: string) => void;
}

export function memorySession(): Session {
  let current = "";
  return {
    token: () => current,
    keep: (token) => {
      current = token;
    },
  };
}
