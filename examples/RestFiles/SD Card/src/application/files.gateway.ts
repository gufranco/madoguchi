import type { FileEntry } from "../domain/file-entry.entity.ts";
import type { Upload } from "./upload.value.ts";

export interface FilesGateway {
  list(): Promise<readonly FileEntry[]>;
  remove(name: string): Promise<void>;
  upload(upload: Upload): Promise<void>;
}
