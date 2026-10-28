import { isCardName } from "@madoguchi/kit/names";

function from(name: string): string | undefined {
  return isCardName(name) ? name : undefined;
}

export const CardName = { from } as const;
