export interface PatchFields {
  readonly name: string;
  readonly state: string;
}

export interface ItemPatch {
  readonly name?: string;
  readonly on?: boolean;
}

function from(fields: PatchFields): ItemPatch {
  const named = fields.name === "" ? {} : { name: fields.name };
  const switched = fields.state === "keep" ? {} : { on: fields.state === "on" };
  return { ...named, ...switched };
}

function changesNothing(patch: ItemPatch): boolean {
  return Object.keys(patch).length === 0;
}

export const ItemPatch = { from, changesNothing } as const;
