export interface Greeting {
  readonly message: string;
}

function isOne(value: unknown): value is Greeting {
  return (
    typeof value === "object" &&
    value !== null &&
    typeof (value as Partial<Greeting>).message === "string"
  );
}

export const Greeting = { isOne } as const;
