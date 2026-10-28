import { inputOf, run, say } from "@madoguchi/kit/page";

import type { FilesView } from "../../application/files.view.ts";
import type { FileEntry } from "../../domain/file-entry.entity.ts";
import { fileUrl } from "../http/file-url.ts";

function row(
  document: Document,
  file: FileEntry,
  remove: (name: string) => Promise<void>,
): HTMLLIElement {
  const entry = document.createElement("li");
  const link = document.createElement("a");
  link.setAttribute("href", `${fileUrl(file.name)}?download=1`);
  link.append(file.name);
  const size = document.createElement("span");
  size.append(`${file.size} bytes`);
  const button = document.createElement("button");
  button.setAttribute("type", "button");
  button.append(`Delete ${file.name}`);
  button.addEventListener("click", () => {
    run(document, () => remove(file.name));
  });
  entry.append(link, " ", size, " ", button);
  return entry;
}

export function createDomFilesView(document: Document): FilesView {
  return {
    chosenFile: () => inputOf(document, "file").files?.[0],
    showFiles: (files, remove) => {
      document
        .getElementById("files")
        ?.replaceChildren(...files.map((file) => row(document, file, remove)));
      say(document, files.length === 1 ? "1 file." : `${files.length} files.`);
    },
    showProgress: (loaded, total) => {
      const progress = document.getElementById("progress");
      progress?.setAttribute("max", String(Math.max(total, 1)));
      progress?.setAttribute("value", String(loaded));
    },
    askForAValidName: () => {
      say(
        document,
        'Choose a file named with 1 to 64 ASCII characters and none of " * / : < > ? \\ | %.',
      );
    },
    showUploaded: (name) => {
      say(document, `Uploaded ${name}.`);
    },
  };
}
