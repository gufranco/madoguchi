import { Window } from "happy-dom";
import { describe, expect, it } from "vitest";

import {
  mountTheme,
  opposite,
  parseTheme,
  preferredTheme,
  saveTheme,
  storedTheme,
  THEME_KEY,
  Theme,
} from "./theme.ts";

function page(): Document {
  const window = new Window();
  window.document.body.innerHTML = '<button type="button" data-theme-toggle></button>';
  return window.document as unknown as Document;
}

function memoryStorage(initial: Readonly<Record<string, string>>): Pick<
  Storage,
  "getItem" | "setItem"
> & {
  readonly items: () => Readonly<Record<string, string>>;
} {
  let items: Readonly<Record<string, string>> = { ...initial };
  return {
    getItem: (key) => items[key] ?? null,
    setItem: (key, value) => {
      items = { ...items, [key]: value };
    },
    items: () => items,
  };
}

const prefers = (dark: boolean) => () => ({ matches: dark });

describe("theme", () => {
  it("reads only the two known themes", () => {
    // Arrange
    const values = ["light", "dark", "blue", null, undefined];

    // Act
    const parsed = values.map((value) => parseTheme(value));

    // Assert
    expect(parsed).toEqual([Theme.Light, Theme.Dark, undefined, undefined, undefined]);
  });

  it("follows the system preference until the visitor chooses", () => {
    // Arrange
    const document = page();
    const storage = memoryStorage({});

    // Act
    const first = mountTheme({ document, storage, match: prefers(true) });

    // Assert
    expect(first).toBe(Theme.Dark);
    expect(document.documentElement.getAttribute("data-theme")).toBe("dark");
    expect(document.querySelector("button")?.getAttribute("aria-pressed")).toBe("true");
    expect(document.querySelector("button")?.textContent).toBe("Switch to light mode");
  });

  it("keeps a stored choice over the system preference", () => {
    // Arrange
    const document = page();
    const storage = memoryStorage({ [THEME_KEY]: Theme.Light });

    // Act
    const first = mountTheme({ document, storage, match: prefers(true) });

    // Assert
    expect(first).toBe(Theme.Light);
    expect(document.documentElement.getAttribute("data-theme")).toBe("light");
  });

  it("switches and remembers the theme when the button is pressed", () => {
    // Arrange
    const document = page();
    const storage = memoryStorage({});
    mountTheme({ document, storage, match: prefers(false) });

    // Act
    document.querySelector<HTMLButtonElement>("button")?.click();

    // Assert
    expect(document.documentElement.getAttribute("data-theme")).toBe("dark");
    expect(storage.items()[THEME_KEY]).toBe("dark");
  });

  it("survives storage that throws", () => {
    // Arrange
    const broken = {
      getItem: () => {
        throw new Error("blocked");
      },
      setItem: () => {
        throw new Error("blocked");
      },
    };

    // Act
    const stored = storedTheme(broken);

    // Assert
    expect(stored).toBeUndefined();
    expect(saveTheme(broken, Theme.Dark)).toBe(false);
    expect(saveTheme(undefined, Theme.Dark)).toBe(false);
  });

  it("works on a page without a toggle", () => {
    // Arrange
    const document = new Window().document as unknown as Document;

    // Act
    const first = mountTheme({ document, storage: undefined, match: prefers(false) });

    // Assert
    expect(first).toBe(Theme.Light);
    expect(preferredTheme(prefers(true))).toBe(Theme.Dark);
    expect(opposite(Theme.Light)).toBe(Theme.Dark);
  });

  it("falls back to the first theme when the page attribute was cleared", () => {
    // Arrange
    const document = page();
    const storage = memoryStorage({});
    mountTheme({ document, storage, match: prefers(true) });
    document.documentElement.removeAttribute("data-theme");

    // Act
    document.querySelector<HTMLButtonElement>("button")?.click();

    // Assert
    expect(document.documentElement.getAttribute("data-theme")).toBe("light");
    expect(opposite(Theme.Dark)).toBe(Theme.Light);
  });
});
