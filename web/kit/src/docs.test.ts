import { describe, expect, it } from "vitest";

import { DOCS_OPTIONS, mountDocs } from "./docs.ts";

describe("docs", () => {
  it("points Swagger UI at the spec the board serves", () => {
    // Arrange
    let received: unknown;
    const host = {
      SwaggerUIBundle: (options: unknown) => {
        received = options;
      },
    };

    // Act
    mountDocs(host);

    // Assert
    expect(received).toEqual(DOCS_OPTIONS);
    expect(DOCS_OPTIONS).toEqual({
      url: "/api/openapi.json",
      dom_id: "#swagger-ui",
      validatorUrl: null,
      persistAuthorization: false,
      tryItOutEnabled: true,
    });
  });

  it("names the missing script when the CDN did not load", () => {
    // Arrange
    const host = {};

    // Act
    const mount = () => mountDocs(host);

    // Assert
    expect(mount).toThrow("Swagger UI did not load from the CDN");
  });
});
