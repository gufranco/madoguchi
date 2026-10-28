import { defineConfig, devices } from "@playwright/test";

import { CARDS, cardFolder } from "./cards.ts";

const SERVER_TIMEOUT_MS = 60000;

export default defineConfig({
  testDir: ".",
  testMatch: "*.e2e.ts",
  fullyParallel: true,
  forbidOnly: true,
  retries: 0,
  reporter: "list",
  use: { trace: "off" },
  projects: [
    {
      name: "phone",
      use: { ...devices["Desktop Chrome"], viewport: { width: 320, height: 640 } },
    },
    {
      name: "desktop",
      use: { ...devices["Desktop Chrome"], viewport: { width: 1280, height: 800 } },
    },
  ],
  webServer: CARDS.map((card) => ({
    command: `pnpm exec vite preview --host 127.0.0.1 --port ${card.port} --strictPort`,
    cwd: cardFolder(card.example),
    url: `http://127.0.0.1:${card.port}/`,
    reuseExistingServer: false,
    timeout: SERVER_TIMEOUT_MS,
  })),
});
