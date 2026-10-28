import {
  apiError,
  describeFailure,
  type Fetcher,
  readJson,
  readProblem,
  sendCommand,
} from "@madoguchi/kit/api";

import type { FilesGateway } from "../../application/files.gateway.ts";
import type { Upload } from "../../application/upload.value.ts";
import { FileEntry } from "../../domain/file-entry.entity.ts";
import { fileUrl } from "./file-url.ts";

async function failure(request: XMLHttpRequest, path: string): Promise<Error> {
  const type = request.getResponseHeader("Content-Type") ?? "";
  const response = new Response(request.responseText, {
    status: request.status,
    headers: { "Content-Type": type },
  });
  const problem = await readProblem(response);
  return apiError(request.status, describeFailure("PUT", path, request.status, problem));
}

function put(Request: typeof XMLHttpRequest, upload: Upload): Promise<void> {
  const path = fileUrl(upload.name);
  return new Promise((done, fail) => {
    const request = new Request();
    request.open("PUT", path);
    request.setRequestHeader("Content-Type", "application/octet-stream");
    request.setRequestHeader("Repr-Digest", upload.digest);
    request.upload.addEventListener("progress", (event) =>
      upload.progress(event.loaded, event.total),
    );
    request.addEventListener("load", () => {
      const stored = request.status === 201 || request.status === 204;
      (stored ? Promise.resolve(undefined) : failure(request, path).then(fail)).then(done, fail);
    });
    request.addEventListener("error", () => fail(new Error(`PUT ${path} did not finish`)));
    request.send(upload.file);
  });
}

export function createHttpFilesGateway(
  fetcher: Fetcher,
  Request: typeof XMLHttpRequest,
): FilesGateway {
  return {
    list: () => readJson(fetcher, "GET", "/api/files", FileEntry.isList),
    remove: (name) => sendCommand(fetcher, "DELETE", fileUrl(name)),
    upload: (upload) => put(Request, upload),
  };
}
