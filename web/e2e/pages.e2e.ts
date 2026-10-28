import { readFileSync } from "node:fs";
import { expect, type Page, type Route, test } from "@playwright/test";

import { CARDS, type Card, type FakeAnswer, specFile } from "./cards.ts";

const TOGGLE_NAME = /^Switch to (dark|light) mode$/;
const METHODS = ["get", "put", "post", "delete", "patch"];

interface Watch {
  readonly problems: () => readonly string[];
}

async function fakeBoard(page: Page, card: Card): Promise<void> {
  const spec = readFileSync(specFile(card.example), "utf8");
  const answers: Readonly<Record<string, FakeAnswer>> = {
    ...card.answers,
    "GET /api/openapi.json": { status: 200, body: JSON.parse(spec) },
  };
  await page.route("**/api/**", (route: Route) => {
    const request = route.request();
    const answer = answers[`${request.method()} ${new URL(request.url()).pathname}`];
    return route.fulfill(
      answer === undefined
        ? {
            status: 404,
            contentType: "application/problem+json",
            json: { title: "Not Found", status: 404 },
          }
        : { status: answer.status, contentType: "application/json", json: answer.body },
    );
  });
}

function watch(page: Page): Watch {
  let problems: readonly string[] = [];
  page.on("console", (message) => {
    if (message.type() === "error" || message.type() === "warning") {
      problems = [...problems, `${message.type()}: ${message.text()}`];
    }
  });
  page.on("pageerror", (error) => {
    problems = [...problems, `pageerror: ${error.message}`];
  });
  return { problems: () => problems };
}

function operationCount(example: string): number {
  const spec = JSON.parse(readFileSync(specFile(example), "utf8")) as {
    readonly paths: Readonly<Record<string, Readonly<Record<string, unknown>>>>;
  };
  return Object.values(spec.paths)
    .map((item) => Object.keys(item).filter((key) => METHODS.includes(key)).length)
    .reduce((total, count) => total + count, 0);
}

async function overflow(page: Page): Promise<number> {
  return page.evaluate(
    () => document.documentElement.scrollWidth - document.documentElement.clientWidth,
  );
}

async function keyboardFocusIsOn(page: Page, name: RegExp): Promise<boolean> {
  await page.keyboard.press("Tab");
  await page.keyboard.press("Shift+Tab");
  const label = await page.evaluate(() => document.activeElement?.textContent ?? "");
  return name.test(label.trim());
}

for (const card of CARDS) {
  const origin = `http://127.0.0.1:${card.port}`;

  test(`${card.example} page renders, themes and focuses`, async ({ page }) => {
    // Arrange
    const seen = watch(page);
    await fakeBoard(page, card);

    // Act
    await page.goto(`${origin}/`);

    // Assert
    await expect(page.getByRole("heading", { level: 1 })).toHaveText(card.heading);
    if (card.status !== undefined) {
      await expect(page.getByRole("status")).toHaveText(card.status);
    }
    expect(await overflow(page)).toBeLessThanOrEqual(0);
    expect(await page.evaluate(() => getComputedStyle(document.body).fontFamily)).toContain(
      "system-ui",
    );
    const toggle = page.getByRole("button", { name: TOGGLE_NAME });
    const before = await page.evaluate(
      () => getComputedStyle(document.documentElement).backgroundColor,
    );
    await toggle.click();
    const after = await page.evaluate(
      () => getComputedStyle(document.documentElement).backgroundColor,
    );
    expect(after).not.toBe(before);
    expect(await keyboardFocusIsOn(page, TOGGLE_NAME)).toBe(true);
    const ring = await page.evaluate(() => {
      const style = getComputedStyle(document.activeElement ?? document.body);
      return `${style.boxShadow}|${style.outlineStyle}`;
    });
    expect(ring).not.toBe("none|none");
    expect(seen.problems()).toEqual([]);
  });

  test(`${card.example} docs page lists every operation`, async ({ page }) => {
    // Arrange
    const seen = watch(page);
    await fakeBoard(page, card);

    // Act
    await page.goto(`${origin}/docs/`);

    // Assert
    await expect(page.locator(".opblock")).toHaveCount(operationCount(card.example));
    expect(await overflow(page)).toBeLessThanOrEqual(0);
    expect(seen.problems()).toEqual([]);
  });
}
