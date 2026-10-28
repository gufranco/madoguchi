export enum Theme {
  Light = "light",
  Dark = "dark",
}

export const THEME_KEY = "madoguchi-theme";

const LABELS: Readonly<Record<Theme, string>> = {
  [Theme.Light]: "Switch to dark mode",
  [Theme.Dark]: "Switch to light mode",
};

export function parseTheme(value: string | null | undefined): Theme | undefined {
  return value === Theme.Light || value === Theme.Dark ? value : undefined;
}

export function storedTheme(storage: Pick<Storage, "getItem"> | undefined): Theme | undefined {
  try {
    return parseTheme(storage?.getItem(THEME_KEY));
  } catch {
    return undefined;
  }
}

export function saveTheme(storage: Pick<Storage, "setItem"> | undefined, theme: Theme): boolean {
  try {
    storage?.setItem(THEME_KEY, theme);
    return storage !== undefined;
  } catch {
    return false;
  }
}

export function preferredTheme(match: (query: string) => Pick<MediaQueryList, "matches">): Theme {
  return match("(prefers-color-scheme: dark)").matches ? Theme.Dark : Theme.Light;
}

export function opposite(theme: Theme): Theme {
  return theme === Theme.Dark ? Theme.Light : Theme.Dark;
}

function showTheme(document: Document, theme: Theme): void {
  document.documentElement.setAttribute("data-theme", theme);
  const button = document.querySelector<HTMLButtonElement>("button[data-theme-toggle]");
  button?.setAttribute("aria-pressed", theme === Theme.Dark ? "true" : "false");
  button?.replaceChildren(LABELS[theme]);
}

export interface ThemeHost {
  readonly document: Document;
  readonly storage: Pick<Storage, "getItem" | "setItem"> | undefined;
  readonly match: (query: string) => Pick<MediaQueryList, "matches">;
}

export function mountTheme(host: ThemeHost): Theme {
  const first = storedTheme(host.storage) ?? preferredTheme(host.match);
  showTheme(host.document, first);
  const button = host.document.querySelector<HTMLButtonElement>("button[data-theme-toggle]");
  button?.addEventListener("click", () => {
    const current = parseTheme(host.document.documentElement.getAttribute("data-theme"));
    const next = opposite(current ?? first);
    showTheme(host.document, next);
    saveTheme(host.storage, next);
  });
  return first;
}
