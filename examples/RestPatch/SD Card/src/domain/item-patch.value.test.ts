import { faker } from "@faker-js/faker";
import { beforeEach, describe, expect, it } from "vitest";

import { ItemPatch } from "./item-patch.value.ts";

describe("ItemPatch", () => {
  beforeEach(() => {
    faker.seed(20261104);
  });

  it("sends only the fields that change", () => {
    // Arrange
    const name = faker.word.noun();

    // Act
    const patch = ItemPatch.from({ name, state: "keep" });

    // Assert
    expect(patch).toEqual({ name });
  });

  it("turns a chosen state into a switch", () => {
    // Arrange
    const fields = { name: "", state: "off" };

    // Act
    const patch = ItemPatch.from(fields);

    // Assert
    expect(patch).toEqual({ on: false });
  });

  it("calls a patch with no field one that changes nothing", () => {
    // Arrange
    const patch = ItemPatch.from({ name: "", state: "keep" });

    // Act
    const empty = ItemPatch.changesNothing(patch);

    // Assert
    expect(empty).toBe(true);
  });
});
