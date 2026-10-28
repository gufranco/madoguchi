import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { faker } from "@faker-js/faker";
import { answer, fakeBoard, problem } from "@madoguchi/kit/testing";
import { beforeEach, describe, expect, it } from "vitest";

import { start } from "./app.ts";

const PAGE = readFileSync(resolve(import.meta.dirname, "../index.html"), "utf8");
const STATUS = '[role="status"]';

describe("RestPost page", () => {
  beforeEach(() => {
    faker.seed(20261102);
  });

  it("creates an item from the form", async () => {
    // Arrange
    const name = faker.word.noun().slice(0, 16);
    const board = fakeBoard(PAGE, [answer(201, { id: 3, name, on: true })]);
    start(board.host);
    board.input("item-name").value = name;
    board.input("item-on").checked = true;

    // Act
    board.submit("form");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("Created item 3.");
    const [call] = board.calls();
    expect(`${call?.method} ${call?.path}`).toBe("POST /api/items");
    expect(call?.headers["content-type"]).toBe("application/json");
    expect(JSON.parse(String(call?.body))).toEqual({ name, on: true });
    expect(board.text("#result")).toBe(`#3 ${name}: on`);
  });

  it("shows why the board refused the item", async () => {
    // Arrange
    const detail = faker.lorem.sentence();
    const board = fakeBoard(PAGE, [problem(507, detail)]);
    start(board.host);
    board.input("item-name").value = faker.word.noun().slice(0, 16);

    // Act
    board.submit("form");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe(`POST /api/items answered 507: ${detail}`);
    expect(board.text("#result")).toBe("");
  });
});
