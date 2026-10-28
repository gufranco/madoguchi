import { createHash } from "node:crypto";
import { faker } from "@faker-js/faker";
import { beforeEach, describe, expect, it } from "vitest";

import { createSubtleDigester } from "./adapters/crypto/digester.subtle.ts";
import { CardName } from "./domain/card-name.value.ts";
import { FileEntry } from "./domain/file-entry.entity.ts";

describe("files", () => {
  beforeEach(() => {
    faker.seed(20261109);
  });

  it.each([
    ["log.txt", "log.txt"],
    ["README", "README"],
    ["holiday photo (2).jpeg", "holiday photo (2).jpeg"],
  ])("accepts %s as %s", (name, expected) => {
    // Arrange
    const chosen = name;

    // Act
    const stored = CardName.from(chosen);

    // Assert
    expect(stored).toBe(expected);
  });

  it.each(["x".repeat(65), ".hidden", "a?b.txt", "trailing.", "caf\u00e9.txt", ""])(
    "refuses %j",
    (name) => {
      // Arrange
      const chosen = name;

      // Act
      const stored = CardName.from(chosen);

      // Assert
      expect(stored).toBeUndefined();
    },
  );

  it("encodes the SHA-256 of the bytes as RFC 9530 asks", async () => {
    // Arrange
    const bytes = new TextEncoder().encode("hello");
    const subtle = {
      digest: async (_: string, data: BufferSource) =>
        Uint8Array.from(
          createHash("sha256")
            .update(new Uint8Array(data as ArrayBuffer))
            .digest(),
        ).buffer,
    };

    // Act
    const header = await createSubtleDigester(subtle).reprDigest(bytes.buffer);

    // Assert
    expect(header).toBe("sha-256=:LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmCQ=:");
  });

  it("accepts only lists of file entries", () => {
    // Arrange
    const entry = { name: faker.system.commonFileName("txt"), size: 3, directory: false };

    // Act
    const accepted = FileEntry.isList([entry]);

    // Assert
    expect(accepted).toBe(true);
    expect(FileEntry.isList([{ ...entry, size: "3" }])).toBe(false);
    expect(FileEntry.isList([null])).toBe(false);
    expect(FileEntry.isList(entry)).toBe(false);
  });
});
