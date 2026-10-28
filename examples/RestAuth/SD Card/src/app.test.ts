import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { faker } from "@faker-js/faker";
import { answer, fakeBoard, problem } from "@madoguchi/kit/testing";
import { beforeEach, describe, expect, it } from "vitest";

import { start } from "./app.ts";

const PAGE = readFileSync(resolve(import.meta.dirname, "../index.html"), "utf8");
const STATUS = '[role="status"]';

function tokenAnswer(token: string): Response {
  return answer(200, { access_token: token, token_type: "Bearer", expires_in: 900 });
}

describe("RestAuth page", () => {
  beforeEach(() => {
    faker.seed(20261107);
  });

  it("signs in, clears the password and sends the token on the protected call", async () => {
    // Arrange
    const token = faker.string.alphanumeric(40);
    const message = faker.lorem.sentence();
    const board = fakeBoard(PAGE, [tokenAnswer(token), answer(200, { message })]);
    start(board.host);
    board.input("username").value = faker.internet.username().slice(0, 16);
    board.input("password").value = faker.internet.password({ length: 20 });
    board.submit("#login");
    await expect.poll(() => board.text(STATUS)).toBe("Signed in.");

    // Act
    board.click("#greet");

    // Assert
    await expect.poll(() => board.text("#greeting")).toBe(message);
    expect(board.input("password").value).toBe("");
    expect(board.calls()[1]?.headers["authorization"]).toBe(`Bearer ${token}`);
  });

  it("calls the protected route without a token before signing in", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [problem(401, "a bearer token is required")]);
    start(board.host);

    // Act
    board.click("#greet");

    // Assert
    await expect
      .poll(() => board.text(STATUS))
      .toBe("GET /api/greeting answered 401: a bearer token is required");
    expect(board.calls()[0]?.headers["authorization"]).toBeUndefined();
  });

  it("shows a refused sign in", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [problem(429, "too many failed logins")]);
    start(board.host);
    board.input("username").value = faker.internet.username().slice(0, 16);
    board.input("password").value = faker.internet.password({ length: 12 });

    // Act
    board.submit("#login");

    // Assert
    await expect
      .poll(() => board.text(STATUS))
      .toBe("POST /api/auth/login answered 429: too many failed logins");
  });
});
