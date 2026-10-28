import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { faker } from "@faker-js/faker";
import { answer, fakeBoard, problem, signInOn, tokenAnswer } from "@madoguchi/kit/testing";
import { beforeEach, describe, expect, it } from "vitest";

import { start } from "./app.ts";

const PAGE = readFileSync(resolve(import.meta.dirname, "../index.html"), "utf8");
const STATUS = '[role="status"]';

describe("RestGet page", () => {
  beforeEach(() => {
    faker.seed(20261101);
  });

  it("lists the items on load", async () => {
    // Arrange
    const name = faker.word.noun();
    const board = fakeBoard(PAGE, [tokenAnswer(), answer(200, [{ id: 1, name, on: true }])]);

    // Act
    start(board.host);
    signInOn(board);

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("1 item.");
    expect(board.calls().map((call) => `${call.method} ${call.path}`)).toEqual([
      "POST /api/auth/login",
      "GET /api/items",
    ]);
    expect(board.text("#items")).toBe(`#1 ${name}: on`);
  });

  it("reads one item by id", async () => {
    // Arrange
    const name = faker.word.noun();
    const board = fakeBoard(PAGE, [
      tokenAnswer(),
      answer(200, []),
      answer(200, { id: 2, name, on: false }),
    ]);
    start(board.host);
    signInOn(board);
    await expect.poll(() => board.text(STATUS)).toBe("0 items.");
    board.input("item-id").value = "2";

    // Act
    board.submit("#find");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("Found item 2.");
    expect(board.calls()[2]?.path).toBe("/api/items/2");
    expect(board.text("#found")).toBe(`#2 ${name}: off`);
  });

  it("shows the problem the board answers", async () => {
    // Arrange
    const detail = faker.lorem.sentence();
    const board = fakeBoard(PAGE, [tokenAnswer(), answer(200, []), problem(404, detail)]);
    start(board.host);
    signInOn(board);
    await expect.poll(() => board.text(STATUS)).toBe("0 items.");
    board.input("item-id").value = "3";

    // Act
    board.submit("#find");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe(`GET /api/items/3 answered 404: ${detail}`);
    expect(board.text("#found")).toBe("");
  });

  it("reports a list of the wrong shape", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [tokenAnswer(), answer(200, { items: [] })]);

    // Act
    start(board.host);
    signInOn(board);

    // Assert
    await expect
      .poll(() => board.text(STATUS))
      .toBe("GET /api/items answered a body of the wrong shape");
  });
});
