import { describe, expect, it } from "vitest";

import { isCardName, isCardPath } from "./names.ts";

describe("card file names", () => {
  it("accepts every name the board can serve", () => {
    // Arrange
    const names = [
      "index.html",
      "docs/index.html",
      "docs/swagger-initializer.js",
      "1a2b3c4d.js.gz",
      "holiday photo (2).jpeg",
      "a".repeat(64),
      `${"b".repeat(60)}/${"c".repeat(62)}`,
    ];

    // Act
    const accepted = names.map(isCardPath);

    // Assert
    expect(accepted).toEqual([true, true, true, true, true, true, true]);
  });

  it("refuses names the board refuses or cannot fit", () => {
    // Arrange
    const refused = [
      "",
      "a".repeat(65),
      `${"b".repeat(60)}/${"c".repeat(63)}`,
      ".hidden",
      "trailing.",
      "trailing ",
      " leading",
      "a?b",
      "a%20b",
      "a:b",
      "caf\u00e9.txt",
      "docs//a.js",
    ];

    // Act
    const accepted = refused.map(isCardPath);

    // Assert
    expect(accepted).toEqual(refused.map(() => false));
  });
});

describe("card names", () => {
  it("keeps a valid name and refuses one the board cannot store", () => {
    // Arrange
    const names = ["holiday photo (2).jpeg", "LOG.TXT", "a/b.txt", "x".repeat(65), "a|b"];

    // Act
    const accepted = names.map(isCardName);

    // Assert
    expect(accepted).toEqual([true, true, false, false, false]);
  });
});
