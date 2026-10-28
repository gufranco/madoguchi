import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { faker } from "@faker-js/faker";
import { answer, fakeBoard, problem } from "@madoguchi/kit/testing";
import { beforeEach, describe, expect, it } from "vitest";

import { patchOf, start } from "./app.ts";

const PAGE = readFileSync(resolve(import.meta.dirname, "../index.html"), "utf8");
const STATUS = '[role="status"]';

describe("RestPatch page", () => {
  beforeEach(() => {
    faker.seed(20261104);
  });

  it.each([
    ["", "keep", {}],
    ["lamp", "keep", { name: "lamp" }],
    ["", "on", { on: true }],
    ["fan", "off", { name: "fan", on: false }],
  ])("sends only the fields that change: name %j, switch %s", (name, state, expected) => {
    // Arrange
    const fields = { name, state };

    // Act
    const patch = patchOf(fields);

    // Assert
    expect(patch).toEqual(expected);
  });

  it("changes the chosen item", async () => {
    // Arrange
    const name = faker.word.noun().slice(0, 16);
    const board = fakeBoard(PAGE, [answer(200, { id: 2, name, on: true })]);
    start(board.host);
    board.input("item-id").value = "2";
    board.input("item-name").value = name;

    // Act
    board.submit("form");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("Changed item 2.");
    const [call] = board.calls();
    expect(`${call?.method} ${call?.path}`).toBe("PATCH /api/items/2");
    expect(JSON.parse(String(call?.body))).toEqual({ name });
    expect(board.text("#result")).toBe(`#2 ${name}: on`);
  });

  it("asks for a change before sending", async () => {
    // Arrange
    const board = fakeBoard(PAGE, []);
    start(board.host);

    // Act
    board.submit("form");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("Fill in a new name or pick a switch state.");
    expect(board.calls()).toEqual([]);
  });

  it("shows why the board refused the change", async () => {
    // Arrange
    const detail = faker.lorem.sentence();
    const board = fakeBoard(PAGE, [problem(422, detail)]);
    start(board.host);
    board.input("item-name").value = faker.word.noun().slice(0, 16);

    // Act
    board.submit("form");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe(`PATCH /api/items/1 answered 422: ${detail}`);
  });
});
