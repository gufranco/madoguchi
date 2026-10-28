import { isCardName } from "@madoguchi/kit/names";

export interface FileEntry {
  readonly name: string;
  readonly size: number;
  readonly directory: boolean;
}

export function cardName(name: string): string | undefined {
  return isCardName(name) ? name : undefined;
}

function isFileEntry(value: unknown): value is FileEntry {
  if (typeof value !== "object" || value === null) {
    return false;
  }
  const fields = value as Readonly<Record<string, unknown>>;
  return (
    typeof fields["name"] === "string" &&
    Number.isInteger(fields["size"]) &&
    typeof fields["directory"] === "boolean"
  );
}

export function isFileList(value: unknown): value is readonly FileEntry[] {
  return Array.isArray(value) && value.every(isFileEntry);
}

export async function reprDigest(
  subtle: Pick<SubtleCrypto, "digest">,
  bytes: ArrayBuffer,
): Promise<string> {
  const hash = new Uint8Array(await subtle.digest("SHA-256", bytes));
  const text = Array.from(hash, (value) => String.fromCharCode(value)).join("");
  return `sha-256=:${btoa(text)}:`;
}
