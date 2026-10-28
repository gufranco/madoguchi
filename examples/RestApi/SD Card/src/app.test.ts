import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { faker } from "@faker-js/faker";
import { answer, type FakeBoard, fakeBoard, problem } from "@madoguchi/kit/testing";
import { beforeEach, describe, expect, it } from "vitest";

import { start } from "./app.ts";

const PAGE = readFileSync(resolve(import.meta.dirname, "../index.html"), "utf8");
const STATUS = '[role="status"]';

function tokenAnswer(token: string): Response {
  return answer(200, { access_token: token, token_type: "Bearer", expires_in: 900 });
}

async function signedIn(board: FakeBoard): Promise<void> {
  start(board.host);
  await expect.poll(() => board.text(STATUS)).toBe("Sign in to change items.");
  board.input("username").value = faker.internet.username().slice(0, 16);
  board.input("password").value = faker.internet.password({ length: 20 });
  board.submit("#login");
  await expect.poll(() => board.text(STATUS)).toBe("Done.");
}

describe("RestApi page", () => {
  beforeEach(() => {
    faker.seed(20261108);
  });

  it("lists the items for everyone", async () => {
    // Arrange
    const name = faker.word.noun().slice(0, 16);
    const board = fakeBoard(PAGE, [answer(200, [{ id: 1, name, value: 3, on: true }])]);

    // Act
    start(board.host);

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("Sign in to change items.");
    expect(board.text("#items li span")).toBe(`#1 ${name}: 3, on`);
  });

  it("signs in and adds an item with the token", async () => {
    // Arrange
    const token = faker.string.alphanumeric(40);
    const name = faker.word.noun().slice(0, 16);
    const created = { id: 2, name, value: 7, on: false };
    const board = fakeBoard(PAGE, [
      answer(200, []),
      tokenAnswer(token),
      answer(200, []),
      answer(201, created),
      answer(200, [created]),
    ]);
    await signedIn(board);
    board.input("item-name").value = name;
    board.input("item-value").value = "7";

    // Act
    board.submit("#add");

    // Assert
    await expect.poll(() => board.calls().length).toBe(5);
    const add = board.calls()[3];
    expect(`${add?.method} ${add?.path}`).toBe("POST /api/items");
    expect(add?.headers["authorization"]).toBe(`Bearer ${token}`);
    expect(JSON.parse(String(add?.body))).toEqual({ name, value: 7, on: false });
    await expect.poll(() => board.text("#items li span")).toBe(`#2 ${name}: 7, off`);
  });

  it("toggles and deletes an item from its row", async () => {
    // Arrange
    const item = { id: 4, name: faker.word.noun().slice(0, 16), value: 1, on: true };
    const board = fakeBoard(PAGE, [
      answer(200, [item]),
      answer(200, { ...item, on: false }),
      answer(200, [{ ...item, on: false }]),
      answer(204, null),
      answer(200, []),
    ]);
    start(board.host);
    await expect.poll(() => board.text("#items li span")).toBe(`#4 ${item.name}: 1, on`);

    // Act
    board.click('#items button[data-action="toggle"]');
    await expect.poll(() => board.text("#items li span")).toBe(`#4 ${item.name}: 1, off`);
    board.click('#items button[data-action="delete"]');

    // Assert
    await expect.poll(() => board.calls().length).toBe(5);
    expect(board.calls().map((call) => `${call.method} ${call.path}`)).toEqual([
      "GET /api/items",
      "PATCH /api/items/4",
      "GET /api/items",
      "DELETE /api/items/4",
      "GET /api/items",
    ]);
    expect(JSON.parse(String(board.calls()[1]?.body))).toEqual({ on: false });
  });

  it("shows the end of the log with the token", async () => {
    // Arrange
    const token = faker.string.alphanumeric(40);
    const line = `{"level":30,"msg":"${faker.lorem.word()}"}\n`;
    const board = fakeBoard(PAGE, [
      answer(200, []),
      tokenAnswer(token),
      answer(200, []),
      new Response(line, { status: 206 }),
    ]);
    await signedIn(board);

    // Act
    board.click("#show-log");

    // Assert
    await expect.poll(() => board.text("#log")).toBe(line);
    const read = board.calls()[3];
    expect(read?.headers["range"]).toBe("bytes=-4096");
    expect(read?.headers["authorization"]).toBe(`Bearer ${token}`);
  });

  it("shows a refused change", async () => {
    // Arrange
    const item = { id: 2, name: faker.word.noun().slice(0, 16), value: 1, on: true };
    const board = fakeBoard(PAGE, [
      answer(200, [item]),
      problem(401, "a bearer token is required"),
    ]);
    start(board.host);
    await expect.poll(() => board.text("#items li span")).not.toBe("");

    // Act
    board.click('#items button[data-action="delete"]');

    // Assert
    await expect
      .poll(() => board.text(STATUS))
      .toBe("DELETE /api/items/2 answered 401: a bearer token is required");
  });
});
