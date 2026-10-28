import { resolve } from "node:path";

export interface FakeAnswer {
  readonly status: number;
  readonly body: unknown;
}

export interface Card {
  readonly example: string;
  readonly port: number;
  readonly heading: string;
  readonly status: string | undefined;
  readonly answers: Readonly<Record<string, FakeAnswer>>;
}

const FIRST_PORT = 4301;
const EXAMPLES_ROOT = resolve(import.meta.dirname, "../../examples");

const ITEM = { id: 1, name: "lamp", on: true };
const VALUED_ITEM = { id: 1, name: "lamp", value: 3, on: true };
const FILE = { name: "holiday photo (2).jpeg", size: 2048, directory: false };

const CARDS_WITHOUT_PORTS: readonly Omit<Card, "port">[] = [
  {
    example: "RestGet",
    heading: "Read items with GET",
    status: "Sign in to use this page.",
    answers: { "GET /api/items": { status: 200, body: [ITEM] } },
  },
  { example: "RestPost", heading: "Create an item with POST", status: undefined, answers: {} },
  { example: "RestPut", heading: "Replace an item with PUT", status: undefined, answers: {} },
  {
    example: "RestPatch",
    heading: "Change part of an item with PATCH",
    status: undefined,
    answers: {},
  },
  { example: "RestDelete", heading: "Delete an item with DELETE", status: undefined, answers: {} },
  {
    example: "RestAuth",
    heading: "Sign in and call a protected route",
    status: undefined,
    answers: {},
  },
  {
    example: "RestFiles",
    heading: "Files on the SD card",
    status: "1 file.",
    answers: { "GET /api/files": { status: 200, body: [FILE] } },
  },
  {
    example: "RestApi",
    heading: "The full REST API",
    status: "Sign in to change items.",
    answers: { "GET /api/items": { status: 200, body: [VALUED_ITEM] } },
  },
];

export const CARDS: readonly Card[] = CARDS_WITHOUT_PORTS.map((card, index) => ({
  ...card,
  port: FIRST_PORT + index,
}));

export function cardFolder(example: string): string {
  return resolve(EXAMPLES_ROOT, example, "SD Card");
}

export function specFile(example: string): string {
  return resolve(EXAMPLES_ROOT, example, "openapi.json");
}
