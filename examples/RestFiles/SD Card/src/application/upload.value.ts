export interface Upload {
  readonly name: string;
  readonly file: File;
  readonly digest: string;
  readonly progress: (loaded: number, total: number) => void;
}
