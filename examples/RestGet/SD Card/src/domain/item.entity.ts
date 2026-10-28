export interface Item {
  readonly id: number;
  readonly name: string;
  readonly on: boolean;
}

function isRecord(value: unknown): value is Readonly<Record<string, unknown>> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function isOne(value: unknown): value is Item {
  return (
    isRecord(value) &&
    Number.isInteger(value["id"]) &&
    typeof value["name"] === "string" &&
    typeof value["on"] === "boolean"
  );
}

function isList(value: unknown): value is readonly Item[] {
  return Array.isArray(value) && value.every(isOne);
}

function describe(item: Item): string {
  return `#${item.id} ${item.name}: ${item.on ? "on" : "off"}`;
}

export const Item = { isOne, isList, describe } as const;
