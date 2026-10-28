import { memorySession } from "@madoguchi/kit/auth";
import { authorizedFetcher, onSignIn } from "@madoguchi/kit/login";
import { type BoardHost, boardFetcher, onSubmit, run, startPage } from "@madoguchi/kit/page";

import { createSubtleDigester } from "./adapters/crypto/digester.subtle.ts";
import { createDomFilesView } from "./adapters/dom/files.view.dom.ts";
import { createHttpFilesGateway } from "./adapters/http/files.gateway.http.ts";
import { createFilesService } from "./application/files.service.ts";

export type FilesHost = BoardHost & {
  readonly XMLHttpRequest: typeof XMLHttpRequest;
  readonly crypto: { readonly subtle: Pick<SubtleCrypto, "digest"> };
};

export function start(host: FilesHost): void {
  const session = memorySession();
  const files = createFilesService(
    createHttpFilesGateway(
      authorizedFetcher(boardFetcher(host), session),
      host.XMLHttpRequest,
      session.token,
    ),
    createDomFilesView(host.document),
    createSubtleDigester(host.crypto.subtle),
  );
  startPage(host);
  onSubmit(host.document, "#upload", () => files.upload());
  onSignIn(host, session, () => files.refresh());
  run(host.document, () => files.refresh());
}
