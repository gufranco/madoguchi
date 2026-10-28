import { resolve } from "node:path";
import { cardConfig } from "@madoguchi/kit/vite";

export default cardConfig({
  root: import.meta.dirname,
  pages: { main: "index.html", docs: "docs/index.html" },
  spec: resolve(import.meta.dirname, "../openapi.json"),
});
