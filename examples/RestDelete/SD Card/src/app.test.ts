import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { faker } from "@faker-js/faker";
import { answer, fakeBoard, problem, signInOn, tokenAnswer } from "@madoguchi/kit/testing";
import { beforeEach, describe, expect, it } from "vitest";

import { start } from "./app.ts";

const PAGE = readFileSync(resolve(import.meta.dirname, "../index.html"), "utf8");
const STATUS = '[role="status"]';

describe("RestDelete page", () => {
  beforeEach(() => {
    faker.seed(20261105);
  });

  it("deletes the chosen item", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [tokenAnswer(), answer(204, null)]);
    start(board.host);
    signInOn(board);
    await expect.poll(() => board.text(STATUS)).toBe("Signed in.");
    board.input("item-id").value = "2";

    // Act
    board.submit("#remove");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("Deleted item 2.");
    const [, call] = board.calls();
    expect(`${call?.method} ${call?.path}`).toBe("DELETE /api/items/2");
  });

  it("shows why the board refused the delete", async () => {
    // Arrange
    const detail = faker.lorem.sentence();
    const board = fakeBoard(PAGE, [tokenAnswer(), problem(404, detail)]);
    start(board.host);
    signInOn(board);
    await expect.poll(() => board.text(STATUS)).toBe("Signed in.");

    // Act
    board.submit("#remove");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe(`DELETE /api/items/1 answered 404: ${detail}`);
  });
});
