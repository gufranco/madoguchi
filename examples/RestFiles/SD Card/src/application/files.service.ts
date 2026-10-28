import { CardName } from "../domain/card-name.value.ts";
import type { Digester } from "./digester.port.ts";
import type { FilesGateway } from "./files.gateway.ts";
import type { FilesView } from "./files.view.ts";

interface FilesService {
  readonly refresh: () => Promise<void>;
  readonly upload: () => Promise<void>;
}

export function createFilesService(
  gateway: FilesGateway,
  view: FilesView,
  digester: Digester,
): FilesService {
  const remove = async (name: string): Promise<void> => {
    await gateway.remove(name);
    await refresh();
  };
  const refresh = async (): Promise<void> => {
    const entries = await gateway.list();
    view.showFiles(
      entries.filter((entry) => !entry.directory),
      remove,
    );
  };
  const upload = async (): Promise<void> => {
    const file = view.chosenFile();
    const name = file === undefined ? undefined : CardName.from(file.name);
    if (file === undefined || name === undefined) {
      view.askForAValidName();
      return;
    }
    const digest = await digester.reprDigest(await file.arrayBuffer());
    await gateway.upload({ name, file, digest, progress: view.showProgress });
    await refresh();
    view.showUploaded(name);
  };
  return { refresh, upload };
}
