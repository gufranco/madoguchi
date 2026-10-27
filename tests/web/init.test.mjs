import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import vm from "node:vm";

const source = readFileSync(new URL("../../examples/RestApi/www/docs/init.js", import.meta.url), "utf8");

test("points Swagger UI at the spec the board serves", () => {
  // Arrange
  let received = null;
  const context = {
    window: {},
    SwaggerUIBundle: (options) => {
      received = options;
      return "ui";
    },
  };

  // Act
  vm.runInNewContext(source, context);

  // Assert
  assert.deepEqual({ ...received }, {
    url: "/api/openapi.json",
    dom_id: "#swagger-ui",
    validatorUrl: null,
    persistAuthorization: false,
    tryItOutEnabled: true,
  });
  assert.equal(context.window.ui, "ui");
});
