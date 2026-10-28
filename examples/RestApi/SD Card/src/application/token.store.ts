export interface TokenStore {
  readonly token: () => string;
  readonly keep: (token: string) => void;
}
