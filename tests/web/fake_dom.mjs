export function seeded(seed) {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    const mixed = Math.imul(state ^ (state >>> 15), 1 | state);
    return ((mixed ^ (mixed >>> 7)) >>> 0) / 4294967296;
  };
}

export function word(random) {
  return Array.from({ length: 8 }, () => "abcdefghijklmnopqrstuvwxyz"[Math.floor(random() * 26)]).join("");
}

export function fakeElement(tag, fields) {
  let children = [];
  let listeners = {};
  let attributes = {};
  let resets = 0;
  return {
    tag,
    ...(fields ?? {}),
    append: (...nodes) => {
      children = [...children, ...nodes];
    },
    replaceChildren: (...nodes) => {
      children = [...nodes];
    },
    addEventListener: (type, handler) => {
      listeners = { ...listeners, [type]: handler };
    },
    setAttribute: (name, value) => {
      attributes = { ...attributes, [name]: value };
    },
    reset: () => {
      resets += 1;
    },
    nodes: () => children,
    listener: (type) => listeners[type],
    attribute: (name) => attributes[name],
    resets: () => resets,
  };
}

export function text(node) {
  return typeof node === "string" ? node : node.nodes().map(text).join("");
}

export async function settle() {
  for (let round = 0; round < 10; round += 1) {
    await new Promise((resolve) => setImmediate(resolve));
  }
}

export function submit(form) {
  form.listener("submit")({ preventDefault: () => undefined, target: form });
}
