export interface FileEntry {
  readonly name: string;
  readonly size: number;
  readonly directory: boolean;
}

function isOne(value: unknown): value is FileEntry {
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

function isList(value: unknown): value is readonly FileEntry[] {
  return Array.isArray(value) && value.every(isOne);
}

export const FileEntry = { isList } as const;
