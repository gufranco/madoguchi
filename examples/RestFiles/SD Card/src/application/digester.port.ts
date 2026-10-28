export interface Digester {
  reprDigest(bytes: ArrayBuffer): Promise<string>;
}
