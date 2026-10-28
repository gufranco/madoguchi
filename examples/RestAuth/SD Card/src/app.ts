import { memorySession } from "@madoguchi/kit/auth";
import { type BoardHost, boardFetcher, onSubmit, run, startPage } from "@madoguchi/kit/page";

import { createDomAccessView } from "./adapters/dom/access.view.dom.ts";
import { createHttpAccessGateway } from "./adapters/http/access.gateway.http.ts";
import { createAccessService } from "./application/access.service.ts";

export function start(host: BoardHost): void {
  const access = createAccessService(
    createHttpAccessGateway(boardFetcher(host)),
    createDomAccessView(host.document),
    memorySession(),
  );
  startPage(host);
  onSubmit(host.document, "#login", () => access.signIn());
  host.document.getElementById("greet")?.addEventListener("click", () => {
    run(host.document, () => access.greet());
  });
}
