import { faker } from "@faker-js/faker";
import { Window } from "happy-dom";
import { beforeEach, describe, expect, it } from "vitest";

import {
  boardFetcher,
  element,
  inputOf,
  onSubmit,
  report,
  run,
  safeStorage,
  say,
  selectOf,
  startPage,
} from "./page.ts";

function browser(markup: string): Window {
  const window = new Window();
  window.document.body.innerHTML = markup;
  return window;
}

describe("page", () => {
  beforeEach(() => {
    faker.seed(20261031);
  });

  it("writes a message to the status line", () => {
    // Arrange
    const window = browser('<p role="status"></p>');
    const message = faker.lorem.sentence();

    // Act
    say(window.document as unknown as Document, message);

    // Assert
    expect(window.document.querySelector('[role="status"]')?.textContent).toBe(message);
  });

  it("finds elements by id and refuses a missing one", () => {
    // Arrange
    const window = browser('<form id="add"><input id="name"></form>');
    const document = window.document as unknown as Document;

    // Act
    const form = element(
      document,
      "add",
      window.HTMLFormElement as unknown as typeof HTMLFormElement,
    );

    // Assert
    expect(form.id).toBe("add");
    expect(inputOf(document, "name").id).toBe("name");
    expect(() => inputOf(document, "nope")).toThrow("the page has no #nope");
    expect(() =>
      element(document, "name", window.HTMLFormElement as unknown as typeof HTMLFormElement),
    ).toThrow("the page has no #name");
  });

  it("starts the theme from the system preference", () => {
    // Arrange
    const window = browser('<button type="button" data-theme-toggle></button>');

    // Act
    startPage(
      window as unknown as Pick<globalThis.Window, "document" | "localStorage" | "matchMedia">,
    );

    // Assert
    expect(window.document.documentElement.getAttribute("data-theme")).toMatch(/^(light|dark)$/);
  });

  it("returns no storage when the browser refuses it", () => {
    // Arrange
    const blocked = {
      get localStorage(): Storage {
        throw new Error("blocked");
      },
    };

    // Act
    const storage = safeStorage(blocked);

    // Assert
    expect(storage).toBeUndefined();
  });

  it("refuses an input lookup on a document without a window", () => {
    // Arrange
    const detached = new Window().document.implementation.createHTMLDocument();

    // Act
    const lookup = () => inputOf(detached as unknown as Document, "name");

    // Assert
    expect(lookup).toThrow("the page has no #name");
  });

  it("reports any failure in the status line", () => {
    // Arrange
    const window = browser('<p role="status"></p>');
    const document = window.document as unknown as Document;
    const message = faker.lorem.words(3);

    // Act
    report(document, new Error(message));

    // Assert
    expect(window.document.querySelector('[role="status"]')?.textContent).toBe(message);
    report(document, "plain text");
    expect(window.document.querySelector('[role="status"]')?.textContent).toBe("plain text");
  });

  it("runs an action and reports its failure", async () => {
    // Arrange
    const window = browser('<p role="status"></p>');
    const document = window.document as unknown as Document;
    const message = faker.lorem.words(2);

    // Act
    run(document, async () => {
      throw new Error(message);
    });

    // Assert
    await expect
      .poll(() => window.document.querySelector('[role="status"]')?.textContent)
      .toBe(message);
  });

  it("wires a form submit to an action without reloading", async () => {
    // Arrange
    const window = browser('<form><button type="submit">go</button></form><p role="status"></p>');
    const document = window.document as unknown as Document;
    let runs = 0;
    onSubmit(document, "form", async () => {
      runs += 1;
    });

    // Act
    window.document.querySelector("form")?.requestSubmit();

    // Assert
    await expect.poll(() => runs).toBe(1);
  });

  it("calls the host fetch with the host as its receiver", async () => {
    // Arrange
    let receiver: unknown;
    const host = {
      fetch(this: unknown, path: string) {
        receiver = this;
        return Promise.resolve(new Response(path));
      },
    };

    // Act
    const response = await boardFetcher(host)("/api/x", {});

    // Assert
    expect(await response.text()).toBe("/api/x");
    expect(receiver).toBe(host);
  });

  it("finds a select box and refuses another element", () => {
    // Arrange
    const window = browser(
      '<select id="state"><option value="on">On</option></select><input id="name">',
    );
    const document = window.document as unknown as Document;

    // Act
    const select = selectOf(document, "state");

    // Assert
    expect(select.value).toBe("on");
    expect(() => selectOf(document, "name")).toThrow("the page has no #name");
  });
});
