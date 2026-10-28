import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    environment: "happy-dom",
    include: ["src/**/*.test.ts"],
    coverage: {
      provider: "v8",
      include: ["src/**/*.ts"],
      exclude: ["src/**/*.test.ts", "src/main.ts", "src/docs.ts"],
      thresholds: { statements: 95, branches: 95, functions: 95, lines: 95 },
    },
  },
});
