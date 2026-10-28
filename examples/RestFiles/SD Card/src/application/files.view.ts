import type { FileEntry } from "../domain/file-entry.entity.ts";

export interface FilesView {
  chosenFile(): File | undefined;
  showFiles(files: readonly FileEntry[], remove: (name: string) => Promise<void>): void;
  showProgress(loaded: number, total: number): void;
  askForAValidName(): void;
  showUploaded(name: string): void;
}
