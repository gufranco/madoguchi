export interface Item {
  readonly id: number;
  readonly name: string;
  readonly on: boolean;
  readonly value?: number;
}

function isRecord(value: unknown): value is Readonly<Record<string, unknown>> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

export function isItem(value: unknown): value is Item {
  if (!isRecord(value)) {
    return false;
  }
  const valued = value["value"] === undefined || Number.isInteger(value["value"]);
  return (
    Number.isInteger(value["id"]) &&
    typeof value["name"] === "string" &&
    typeof value["on"] === "boolean" &&
    valued
  );
}

export function isItemList(value: unknown): value is readonly Item[] {
  return Array.isArray(value) && value.every(isItem);
}

export function itemLine(item: Item): string {
  const state = item.on ? "on" : "off";
  const detail = item.value === undefined ? state : `${item.value}, ${state}`;
  return `#${item.id} ${item.name}: ${detail}`;
}
