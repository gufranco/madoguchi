import { faker } from "@faker-js/faker";
import { beforeEach, describe, expect, it } from "vitest";

import { Item } from "./item.entity.ts";

describe("Item", () => {
  beforeEach(() => {
    faker.seed(20261110);
  });

  it("accepts the shape the board answers", () => {
    // Arrange
    const value = {
      id: faker.number.int({ min: 1, max: 4 }),
      name: faker.word.noun(),
      value: faker.number.int({ min: -100000, max: 100000 }),
      on: true,
    };

    // Act
    const accepted = Item.isOne(value);

    // Assert
    expect(accepted).toBe(true);
  });

  it("refuses an item without a value", () => {
    // Arrange
    const value = { id: 1, name: faker.word.noun(), on: false };

    // Act
    const accepted = Item.isOne(value);

    // Assert
    expect(accepted).toBe(false);
  });

  it("refuses a list holding one bad entry", () => {
    // Arrange
    const value = [{ id: 1, name: faker.word.noun(), value: 1, on: true }, null];

    // Act
    const accepted = Item.isList(value);

    // Assert
    expect(accepted).toBe(false);
  });

  it("describes an item by id, name, value and state", () => {
    // Arrange
    const name = faker.word.noun();
    const amount = faker.number.int({ min: -9, max: 9 });

    // Act
    const line = Item.describe({ id: 2, name, value: amount, on: false });

    // Assert
    expect(line).toBe(`#2 ${name}: ${amount}, off`);
  });
});
