import { describe, expect, it } from "vitest";

import { CDN_FILES } from "./cdn.ts";

const PINNED =
  /^https:\/\/(cdn\.jsdelivr\.net|unpkg\.com)\/(npm\/)?[@\w./-]+@\d+\.\d+\.\d+\/[\w./-]+$/;
const SRI = /^sha384-[A-Za-z0-9+/]{64}$/;

describe("CDN files", () => {
  it.each(Object.entries(CDN_FILES))(
    "pins %s to an exact version with an integrity hash",
    (_, file) => {
      // Arrange
      const { href, integrity } = file;

      // Act
      const pinned = PINNED.test(href);

      // Assert
      expect(pinned).toBe(true);
      expect(SRI.test(integrity)).toBe(true);
    },
  );

  it("lists Pico CSS classless and Swagger UI", () => {
    // Arrange
    const names = Object.keys(CDN_FILES);

    // Act
    const sorted = names.toSorted();

    // Assert
    expect(sorted).toEqual(["PICO_CSS", "SWAGGER_CSS", "SWAGGER_JS"]);
    expect(CDN_FILES["PICO_CSS"]?.href.endsWith("/pico.classless.min.css")).toBe(true);
  });
});
