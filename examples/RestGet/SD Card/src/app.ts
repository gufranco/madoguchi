import { type BoardHost, boardFetcher, onSubmit, run, startPage } from "@madoguchi/kit/page";

import { createDomItemsView } from "./adapters/dom/items.view.dom.ts";
import { createHttpItemGateway } from "./adapters/http/item.gateway.http.ts";
import { createItemsService } from "./application/items.service.ts";

export function start(host: BoardHost): void {
  const items = createItemsService(
    createHttpItemGateway(boardFetcher(host)),
    createDomItemsView(host.document),
  );
  startPage(host);
  onSubmit(host.document, "form", () => items.findRequested());
  run(host.document, () => items.load());
}
