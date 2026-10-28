import { memorySession } from "@madoguchi/kit/auth";
import { type BoardHost, boardFetcher, onSubmit, run, startPage } from "@madoguchi/kit/page";

import { createDomBoardView } from "./adapters/dom/board.view.dom.ts";
import { createHttpBoardGateway } from "./adapters/http/board.gateway.http.ts";
import { createBoardService } from "./application/board.service.ts";

export function start(host: BoardHost): void {
  const board = createBoardService(
    createHttpBoardGateway(boardFetcher(host)),
    createDomBoardView(host.document),
    memorySession(),
  );
  startPage(host);
  onSubmit(host.document, "#login", () => board.signIn());
  onSubmit(host.document, "#add", () => board.add());
  host.document.getElementById("show-log")?.addEventListener("click", () => {
    run(host.document, () => board.showLog());
  });
  run(host.document, () => board.refresh());
}
