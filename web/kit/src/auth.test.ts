import { faker } from "@faker-js/faker";
import { beforeEach, describe, expect, it } from "vitest";

import { bearer, isToken, memorySession, signIn } from "./auth.ts";
import { answer, problem } from "./testing.ts";

describe("auth", () => {
  beforeEach(() => {
    faker.seed(20261106);
  });

  it("signs in and returns the access token", async () => {
    // Arrange
    const token = faker.string.alphanumeric(40);
    const username = faker.internet.username().slice(0, 16);
    const password = faker.internet.password({ length: 20 });
    let sent: RequestInit = {};
    const fetcher = async (_: string, init: RequestInit) => {
      sent = init;
      return answer(200, { access_token: token, token_type: "Bearer", expires_in: 900 });
    };

    // Act
    const received = await signIn(fetcher, { username, password });

    // Assert
    expect(received).toBe(token);
    expect(sent.method).toBe("POST");
    expect(JSON.parse(String(sent.body))).toEqual({ username, password });
  });

  it("names a refused sign in", async () => {
    // Arrange
    const detail = faker.lorem.sentence();
    const fetcher = async () => problem(401, detail);

    // Act
    const attempt = signIn(fetcher, { username: "a", password: "b" });

    // Assert
    await expect(attempt).rejects.toThrow(`POST /api/auth/login answered 401: ${detail}`);
  });

  it("accepts only a bearer token answer", () => {
    // Arrange
    const good = { access_token: "x", token_type: "Bearer", expires_in: 900 };

    // Act
    const accepted = isToken(good);

    // Assert
    expect(accepted).toBe(true);
    expect(isToken({ ...good, token_type: "Basic" })).toBe(false);
    expect(isToken({ ...good, expires_in: "900" })).toBe(false);
    expect(isToken(null)).toBe(false);
  });

  it("builds the authorization header only for a token", () => {
    // Arrange
    const token = faker.string.alphanumeric(20);

    // Act
    const headers = bearer(token);

    // Assert
    expect(headers).toEqual({ Authorization: `Bearer ${token}` });
    expect(bearer("")).toEqual({});
  });

  it("keeps the token in memory only", () => {
    // Arrange
    const session = memorySession();
    const token = faker.string.alphanumeric(30);

    // Act
    session.keep(token);

    // Assert
    expect(session.token()).toBe(token);
    expect(memorySession().token()).toBe("");
  });
});
