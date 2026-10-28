import { faker } from "@faker-js/faker";
import { beforeEach, describe, expect, it } from "vitest";

import { isItem, isItemList, itemLine } from "./items.ts";

describe("items", () => {
  beforeEach(() => {
    faker.seed(20261030);
  });

  it("accepts an item with or without a value", () => {
    // Arrange
    const plain = { id: faker.number.int({ min: 1, max: 4 }), name: faker.word.noun(), on: true };

    // Act
    const accepted = isItem(plain);

    // Assert
    expect(accepted).toBe(true);
    expect(isItem({ ...plain, value: faker.number.int({ min: -9, max: 9 }) })).toBe(true);
  });

  it.each([
    ["a missing id", { name: "x", on: true }],
    ["a text id", { id: "1", name: "x", on: true }],
    ["a missing name", { id: 1, on: true }],
    ["a text switch", { id: 1, name: "x", on: "yes" }],
    ["a text value", { id: 1, name: "x", on: true, value: "3" }],
    ["null", null],
  ])("refuses %s", (_, value) => {
    // Arrange
    const candidate: unknown = value;

    // Act
    const accepted = isItem(candidate);

    // Assert
    expect(accepted).toBe(false);
  });

  it("accepts only lists of items", () => {
    // Arrange
    const list = [{ id: 1, name: faker.word.noun(), on: false }];

    // Act
    const accepted = isItemList(list);

    // Assert
    expect(accepted).toBe(true);
    expect(isItemList([...list, { id: 2 }])).toBe(false);
    expect(isItemList({ items: list })).toBe(false);
  });

  it("describes an item in one line", () => {
    // Arrange
    const name = faker.word.noun();

    // Act
    const line = itemLine({ id: 3, name, on: true });

    // Assert
    expect(line).toBe(`#3 ${name}: on`);
    expect(itemLine({ id: 4, name, on: false, value: -2 })).toBe(`#4 ${name}: -2, off`);
  });
});
