import type { Digester } from "../../application/digester.port.ts";

export function createSubtleDigester(subtle: Pick<SubtleCrypto, "digest">): Digester {
  return {
    async reprDigest(bytes: ArrayBuffer): Promise<string> {
      const hash = new Uint8Array(await subtle.digest("SHA-256", bytes));
      const text = Array.from(hash, (value) => String.fromCharCode(value)).join("");
      return `sha-256=:${btoa(text)}:`;
    },
  };
}
