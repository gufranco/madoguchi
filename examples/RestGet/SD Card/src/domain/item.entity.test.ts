import { faker } from "@faker-js/faker";
import { beforeEach, describe, expect, it } from "vitest";

import { Item } from "./item.entity.ts";

describe("Item", () => {
  beforeEach(() => {
    faker.seed(20261102);
  });

  it("accepts the shape the board answers", () => {
    // Arrange
    const value = { id: faker.number.int({ min: 1, max: 4 }), name: faker.word.noun(), on: true };

    // Act
    const accepted = Item.isOne(value);

    // Assert
    expect(accepted).toBe(true);
  });

  it("refuses an item without a whole-number id", () => {
    // Arrange
    const value = {
      id: faker.number.float({ min: 1.1, max: 3.9 }),
      name: faker.word.noun(),
      on: false,
    };

    // Act
    const accepted = Item.isOne(value);

    // Assert
    expect(accepted).toBe(false);
  });

  it("refuses a list holding one bad entry", () => {
    // Arrange
    const value = [
      { id: 1, name: faker.word.noun(), on: true },
      { id: 2, name: faker.word.noun() },
    ];

    // Act
    const accepted = Item.isList(value);

    // Assert
    expect(accepted).toBe(false);
  });

  it("refuses an array where one item is expected", () => {
    // Arrange
    const value = [{ id: 1, name: faker.word.noun(), on: true }];

    // Act
    const accepted = Item.isOne(value);

    // Assert
    expect(accepted).toBe(false);
  });

  it("describes an item by id, name and state", () => {
    // Arrange
    const name = faker.word.noun();

    // Act
    const line = Item.describe({ id: 3, name, on: false });

    // Assert
    expect(line).toBe(`#3 ${name}: off`);
  });
});
