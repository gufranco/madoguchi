import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { faker } from "@faker-js/faker";
import { answer, fakeBoard, problem, signInOn, tokenAnswer } from "@madoguchi/kit/testing";
import { beforeEach, describe, expect, it } from "vitest";

import { start } from "./app.ts";

const PAGE = readFileSync(resolve(import.meta.dirname, "../index.html"), "utf8");
const STATUS = '[role="status"]';

describe("RestPut page", () => {
  beforeEach(() => {
    faker.seed(20261103);
  });

  it("replaces every field of the chosen item", async () => {
    // Arrange
    const name = faker.word.noun().slice(0, 16);
    const board = fakeBoard(PAGE, [tokenAnswer(), answer(200, { id: 2, name, on: false })]);
    start(board.host);
    signInOn(board);
    await expect.poll(() => board.text(STATUS)).toBe("Signed in.");
    board.input("item-id").value = "2";
    board.input("item-name").value = name;

    // Act
    board.submit("#replace");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("Replaced item 2.");
    const [, call] = board.calls();
    expect(`${call?.method} ${call?.path}`).toBe("PUT /api/items/2");
    expect(JSON.parse(String(call?.body))).toEqual({ name, on: false });
    expect(board.text("#result")).toBe(`#2 ${name}: off`);
  });

  it("shows why the board refused the replacement", async () => {
    // Arrange
    const detail = faker.lorem.sentence();
    const board = fakeBoard(PAGE, [tokenAnswer(), problem(404, detail)]);
    start(board.host);
    signInOn(board);
    await expect.poll(() => board.text(STATUS)).toBe("Signed in.");
    board.input("item-name").value = faker.word.noun().slice(0, 16);

    // Act
    board.submit("#replace");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe(`PUT /api/items/1 answered 404: ${detail}`);
  });
});
