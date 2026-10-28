import { memorySession } from "@madoguchi/kit/auth";
import { authorizedFetcher, onSignIn } from "@madoguchi/kit/login";
import { type BoardHost, boardFetcher, onSubmit, startPage } from "@madoguchi/kit/page";

import { createDomItemsView } from "./adapters/dom/items.view.dom.ts";
import { createHttpItemGateway } from "./adapters/http/item.gateway.http.ts";
import { createItemsService } from "./application/items.service.ts";

export function start(host: BoardHost): void {
  const session = memorySession();
  const items = createItemsService(
    createHttpItemGateway(authorizedFetcher(boardFetcher(host), session)),
    createDomItemsView(host.document),
  );
  startPage(host);
  onSubmit(host.document, "#create", () => items.createRequested());
  onSignIn(host, session, async () => {});
}
