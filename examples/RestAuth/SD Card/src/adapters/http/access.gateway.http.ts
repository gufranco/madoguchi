import { type Fetcher, readJson } from "@madoguchi/kit/api";
import { bearer, signIn } from "@madoguchi/kit/auth";

import type { AccessGateway } from "../../application/access.gateway.ts";
import { Greeting } from "../../domain/greeting.entity.ts";

export function createHttpAccessGateway(fetcher: Fetcher): AccessGateway {
  return {
    signIn: (credentials) => signIn(fetcher, credentials),
    greeting: (token) =>
      readJson(fetcher, "GET", "/api/greeting", Greeting.isOne, { headers: bearer(token) }),
  };
}
