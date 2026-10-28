import {
  apiError,
  describeFailure,
  type Fetcher,
  readJson,
  readProblem,
  sendCommand,
} from "@madoguchi/kit/api";
import {
  type BoardHost,
  boardFetcher,
  inputOf,
  onSubmit,
  run,
  say,
  startPage,
} from "@madoguchi/kit/page";

import { cardName, type FileEntry, isFileList, reprDigest } from "./files.ts";

export type FilesHost = BoardHost & {
  readonly XMLHttpRequest: typeof XMLHttpRequest;
  readonly crypto: { readonly subtle: Pick<SubtleCrypto, "digest"> };
};

interface Context {
  readonly host: FilesHost;
  readonly document: Document;
  readonly fetcher: Fetcher;
}

const fileUrl = (name: string) => `/api/files/${encodeURIComponent(name)}`;

function row(context: Context, file: FileEntry): HTMLLIElement {
  const entry = context.document.createElement("li");
  const link = context.document.createElement("a");
  link.setAttribute("href", `${fileUrl(file.name)}?download=1`);
  link.append(file.name);
  const size = context.document.createElement("span");
  size.append(`${file.size} bytes`);
  const remove = context.document.createElement("button");
  remove.setAttribute("type", "button");
  remove.append(`Delete ${file.name}`);
  remove.addEventListener("click", () => {
    run(context.document, async () => {
      await sendCommand(context.fetcher, "DELETE", fileUrl(file.name));
      await refresh(context);
    });
  });
  entry.append(link, " ", size, " ", remove);
  return entry;
}

async function refresh(context: Context): Promise<void> {
  const entries = await readJson(context.fetcher, "GET", "/api/files", isFileList);
  const files = entries.filter((entry) => !entry.directory);
  context.document
    .getElementById("files")
    ?.replaceChildren(...files.map((file) => row(context, file)));
  say(context.document, files.length === 1 ? "1 file." : `${files.length} files.`);
}

function showProgress(document: Document, loaded: number, total: number): void {
  const progress = document.getElementById("progress");
  progress?.setAttribute("max", String(Math.max(total, 1)));
  progress?.setAttribute("value", String(loaded));
}

async function failure(request: XMLHttpRequest, path: string): Promise<Error> {
  const type = request.getResponseHeader("Content-Type") ?? "";
  const response = new Response(request.responseText, {
    status: request.status,
    headers: { "Content-Type": type },
  });
  const problem = await readProblem(response);
  return apiError(request.status, describeFailure("PUT", path, request.status, problem));
}

function send(context: Context, name: string, file: File, digest: string): Promise<void> {
  const path = fileUrl(name);
  return new Promise((done, fail) => {
    const request = new context.host.XMLHttpRequest();
    request.open("PUT", path);
    request.setRequestHeader("Content-Type", "application/octet-stream");
    request.setRequestHeader("Repr-Digest", digest);
    request.upload.addEventListener("progress", (event) =>
      showProgress(context.document, event.loaded, event.total),
    );
    request.addEventListener("load", () => {
      const stored = request.status === 201 || request.status === 204;
      (stored ? Promise.resolve(undefined) : failure(request, path).then(fail)).then(done, fail);
    });
    request.addEventListener("error", () => fail(new Error(`PUT ${path} did not finish`)));
    request.send(file);
  });
}

async function upload(context: Context): Promise<void> {
  const file = inputOf(context.document, "file").files?.[0];
  const name = file === undefined ? undefined : cardName(file.name);
  if (file === undefined || name === undefined) {
    say(
      context.document,
      'Choose a file named with 1 to 64 ASCII characters and none of " * / : < > ? \\ | %.',
    );
    return;
  }
  const digest = await reprDigest(context.host.crypto.subtle, await file.arrayBuffer());
  await send(context, name, file, digest);
  await refresh(context);
  say(context.document, `Uploaded ${name}.`);
}

export function start(host: FilesHost): void {
  const context: Context = { host, document: host.document, fetcher: boardFetcher(host) };
  startPage(host);
  onSubmit(host.document, "#upload", () => upload(context));
  run(host.document, () => refresh(context));
}
