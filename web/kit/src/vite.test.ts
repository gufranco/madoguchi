import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { gunzipSync } from "node:zlib";
import { faker } from "@faker-js/faker";
import { build } from "vite";
import { beforeEach, describe, expect, it } from "vitest";

import { PICO_CSS } from "./cdn.ts";
import { isCardPath } from "./names.ts";
import { cardConfig, fillCdn, finishCard, listFiles, pagePolicy, withPolicy } from "./vite.ts";

const PAGE = `<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <title>Fixture</title>
    <link rel="stylesheet" href="%PICO_CSS_HREF%" integrity="%PICO_CSS_INTEGRITY%" crossorigin="anonymous">
    <script type="module" src="./main.ts"></script>
  </head>
  <body>
    <main><h1>Fixture</h1><p id="out"></p></main>
  </body>
</html>
`;

function fixture(): string {
  const root = mkdtempSync(join(tmpdir(), "madoguchi-card-"));
  writeFileSync(join(root, "index.html"), PAGE);
  writeFileSync(
    join(root, "main.ts"),
    `document.getElementById("out")?.replaceChildren("${faker.word.noun()}");\n`,
  );
  mkdirSync(join(root, "docs"));
  writeFileSync(join(root, "docs", "index.html"), PAGE.replace("./main.ts", "../main.ts"));
  writeFileSync(
    join(root, "openapi.json"),
    JSON.stringify({ openapi: "3.1.0", info: { title: "x" } }, null, 2),
  );
  return root;
}

describe("page policy", () => {
  it("fills CDN placeholders with pinned files", () => {
    // Arrange
    const html = '<link href="%PICO_CSS_HREF%" integrity="%PICO_CSS_INTEGRITY%">';

    // Act
    const filled = fillCdn(html);

    // Assert
    expect(filled).toBe(`<link href="${PICO_CSS.href}" integrity="${PICO_CSS.integrity}">`);
    expect(() => fillCdn("%NOPE_HREF%")).toThrow("unknown CDN placeholder %NOPE_HREF%");
  });

  it("allows only the CDN origins a page loads", () => {
    // Arrange
    const html = fillCdn(PAGE).replace(
      "</head>",
      '<script src="https://unpkg.com/swagger-ui-dist@5.33.2/swagger-ui-bundle.js"></script></head>',
    );

    // Act
    const policy = pagePolicy(html);

    // Assert
    expect(policy).toBe(
      "default-src 'none'; script-src 'self' https://unpkg.com; style-src 'self' https://cdn.jsdelivr.net; " +
        "img-src 'self' data:; connect-src 'self'; font-src 'self'; base-uri 'none'; form-action 'self'",
    );
  });

  it("writes the policy right after the charset", () => {
    // Arrange
    const html = '<head><meta charset="utf-8"><title>x</title></head>';

    // Act
    const placed = withPolicy(html);

    // Assert
    expect(placed).toBe(
      '<head><meta charset="utf-8"><meta http-equiv="Content-Security-Policy" content="default-src \'none\'; ' +
        "script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; font-src 'self'; " +
        "base-uri 'none'; form-action 'self'\"><title>x</title></head>",
    );
    expect(() => withPolicy("<head></head>")).toThrow('a page needs <meta charset="utf-8">');
  });
});

describe("card build", () => {
  beforeEach(() => {
    faker.seed(20261029);
  });

  it("writes the pages, gzip copies, the spec and a policy for every page", async () => {
    // Arrange
    const root = fixture();
    const config = cardConfig({
      root,
      pages: { main: "index.html", docs: "docs/index.html" },
      spec: join(root, "openapi.json"),
    });

    // Act
    await build({ ...config, configFile: false, logLevel: "silent" });

    // Assert
    const out = join(root, "dist");
    const files = listFiles(out).toSorted();
    const page = readFileSync(join(out, "index.html"), "utf8");
    expect(files.every(isCardPath)).toBe(true);
    expect(files).toContain("index.html");
    expect(files).toContain("docs/index.html");
    expect(files).toContain("openapi.json");
    expect(files).toContain("index.html.gz");
    expect(files.filter((name) => /^[0-9a-f]{8}\.js$/.test(name))).toHaveLength(1);
    expect(page).toContain(
      `content="${pagePolicy(fillCdn(PAGE))}" http-equiv=Content-Security-Policy`,
    );
    expect(page).toContain(PICO_CSS.integrity);
    expect(page).not.toContain("\n  ");
    expect(gunzipSync(readFileSync(join(out, "index.html.gz"))).toString("utf8")).toBe(page);
    expect(readFileSync(join(out, "openapi.json"), "utf8")).toBe(
      '{"openapi":"3.1.0","info":{"title":"x"}}',
    );
  });

  it("leaves files that do not shrink without a gzip copy and refuses long names", () => {
    // Arrange
    const out = mkdtempSync(join(tmpdir(), "madoguchi-out-"));
    writeFileSync(join(out, "a.js"), "x");
    writeFileSync(join(out, "b.png"), faker.string.alphanumeric(200));

    // Act
    finishCard(out, undefined);

    // Assert
    expect(listFiles(out)).toEqual(["a.js", "b.png"]);
    writeFileSync(join(out, "a?b.js"), "x");
    expect(() => finishCard(out, undefined)).toThrow("a?b.js is not a name the board can serve");
  });

  it("proxies the API to the board it is given", () => {
    // Arrange
    const board = `http://${faker.internet.ipv4()}`;

    // Act
    const config = cardConfig({ root: tmpdir(), pages: { main: "index.html" }, board });

    // Assert
    expect(config.server?.proxy).toEqual({ "/api": { target: board, changeOrigin: true } });
    expect(cardConfig({ root: tmpdir(), pages: {} }).server?.proxy).toEqual({
      "/api": {
        target: process.env["MADOGUCHI_BOARD"] ?? "http://madoguchi.local",
        changeOrigin: true,
      },
    });
  });
});
