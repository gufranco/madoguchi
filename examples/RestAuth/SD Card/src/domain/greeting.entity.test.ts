import { faker } from "@faker-js/faker";
import { beforeEach, describe, expect, it } from "vitest";

import { Greeting } from "./greeting.entity.ts";

describe("Greeting", () => {
  beforeEach(() => {
    faker.seed(20261108);
  });

  it("accepts an object with a message", () => {
    // Arrange
    const value = { message: faker.lorem.sentence() };

    // Act
    const accepted = Greeting.isOne(value);

    // Assert
    expect(accepted).toBe(true);
  });

  it("refuses a message that is not text", () => {
    // Arrange
    const value = { message: faker.number.int() };

    // Act
    const accepted = Greeting.isOne(value);

    // Assert
    expect(accepted).toBe(false);
  });

  it("refuses null", () => {
    // Arrange
    const value = null;

    // Act
    const accepted = Greeting.isOne(value);

    // Assert
    expect(accepted).toBe(false);
  });
});
