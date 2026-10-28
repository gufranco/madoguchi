import { faker } from "@faker-js/faker";
import { beforeEach, describe, expect, it } from "vitest";

import { memorySession } from "./auth.ts";
import { authorizedFetcher, onSignIn, SIGN_IN_FIRST } from "./login.ts";
import { answer, fakeBoard, problem } from "./testing.ts";

const PAGE = `<!doctype html><p role="status"></p>
<form id="login"><input id="username"><input id="password"><button>Sign in</button></form>`;

describe("login", () => {
  beforeEach(() => {
    faker.seed(20261028);
  });

  it("adds the kept token to every request", async () => {
    // Arrange
    const token = faker.string.alphanumeric(40);
    const session = memorySession();
    session.keep(token);
    let sent: RequestInit = {};
    const fetcher = authorizedFetcher(async (_, init) => {
      sent = init;
      return answer(200, {});
    }, session);

    // Act
    await fetcher("/api/items", { method: "GET", headers: { Accept: "application/json" } });

    // Assert
    expect(new Headers(sent.headers).get("Authorization")).toBe(`Bearer ${token}`);
    expect(new Headers(sent.headers).get("Accept")).toBe("application/json");
  });

  it("sends no Authorization header before sign-in", async () => {
    // Arrange
    let sent: RequestInit = {};
    const fetcher = authorizedFetcher(async (_, init) => {
      sent = init;
      return answer(200, {});
    }, memorySession());

    // Act
    await fetcher("/api/items", { method: "GET" });

    // Assert
    expect(new Headers(sent.headers).has("Authorization")).toBe(false);
  });

  it("asks for a sign-in until the form is sent", () => {
    // Arrange
    const board = fakeBoard(PAGE, []);

    // Act
    onSignIn(board.host, memorySession(), async () => {});

    // Assert
    expect(board.text('[role="status"]')).toBe(SIGN_IN_FIRST);
  });

  it("keeps the token and loads the page after a sign-in", async () => {
    // Arrange
    const token = faker.string.alphanumeric(40);
    const board = fakeBoard(PAGE, [
      answer(200, { access_token: token, token_type: "Bearer", expires_in: 900 }),
    ]);
    const session = memorySession();
    let loaded = 0;
    onSignIn(board.host, session, async () => {
      loaded += 1;
    });
    board.input("username").value = "admin";
    board.input("password").value = faker.internet.password({ length: 20 });

    // Act
    board.submit("#login");
    await new Promise((resolve) => setTimeout(resolve, 0));

    // Assert
    expect(session.token()).toBe(token);
    expect(loaded).toBe(1);
    expect(board.calls()[0]?.path).toBe("/api/auth/login");
  });

  it("reports a refused sign-in", async () => {
    // Arrange
    const detail = faker.lorem.sentence();
    const board = fakeBoard(PAGE, [problem(401, detail)]);
    onSignIn(board.host, memorySession(), async () => {});

    // Act
    board.submit("#login");
    await new Promise((resolve) => setTimeout(resolve, 0));

    // Assert
    expect(board.text('[role="status"]')).toBe(`POST /api/auth/login answered 401: ${detail}`);
  });
});
