import { readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { join, posix, resolve } from "node:path";
import { constants, gzipSync } from "node:zlib";
import minifyHtml from "@minify-html/node";
import type { Plugin, UserConfig } from "vite";

import { CDN_FILES } from "./cdn.ts";
import { isCardPath } from "./names.ts";

const GZIP_SUFFIX = ".gz";
const PLACEHOLDER = /%([A-Z_]+)_(HREF|INTEGRITY)%/g;
const SCRIPT_ORIGIN = /<script\b[^>]*\bsrc="(https:\/\/[^/"]+)/g;
const STYLE_ORIGIN = /<link\b[^>]*\bhref="(https:\/\/[^/"]+)[^>]*>/g;
const CHARSET = '<meta charset="utf-8">';
const PACKED_EXTENSIONS: readonly string[] = [".html", ".js", ".css", ".svg", ".json", ".txt"];
const SPEC_NAME = "openapi.json";

export interface CardOptions {
  readonly root: string;
  readonly pages: Readonly<Record<string, string>>;
  readonly spec?: string;
  readonly board?: string;
}

export function fillCdn(html: string): string {
  return html.replace(PLACEHOLDER, (whole, name: string, part: string) => {
    const file = CDN_FILES[name];
    if (file === undefined) {
      throw new Error(`unknown CDN placeholder ${whole}`);
    }
    return part === "HREF" ? file.href : file.integrity;
  });
}

function origins(html: string, pattern: RegExp): string {
  const found = [...html.matchAll(pattern)].map((match) => match[1] ?? "");
  return ["'self'", ...new Set(found.filter((origin) => origin !== ""))].join(" ");
}

export function pagePolicy(html: string): string {
  return [
    "default-src 'none'",
    `script-src ${origins(html, SCRIPT_ORIGIN)}`,
    `style-src ${origins(html, STYLE_ORIGIN)}`,
    "img-src 'self' data:",
    "connect-src 'self'",
    "font-src 'self'",
    "base-uri 'none'",
    "form-action 'self'",
  ].join("; ");
}

export function withPolicy(html: string): string {
  if (!html.includes(CHARSET)) {
    throw new Error(`a page needs ${CHARSET}`);
  }
  const meta = `<meta http-equiv="Content-Security-Policy" content="${pagePolicy(html)}">`;
  return html.replace(CHARSET, `${CHARSET}${meta}`);
}

function minifyPage(html: string): string {
  const options = {
    keep_closing_tags: true,
    keep_html_and_head_opening_tags: true,
    minify_css: true,
  };
  return minifyHtml.minify(Buffer.from(html), options).toString("utf8");
}

export function listFiles(root: string): readonly string[] {
  return readdirSync(root, { recursive: true, encoding: "utf8" })
    .filter((name) => statSync(join(root, name)).isFile())
    .map((name) => name.split("\\").join("/"))
    .toSorted();
}

function minifyPages(out: string): void {
  for (const name of listFiles(out).filter((file) => posix.extname(file) === ".html")) {
    writeFileSync(join(out, name), minifyPage(readFileSync(join(out, name), "utf8")));
  }
}

function writeSpec(out: string, spec: string | undefined): void {
  if (spec !== undefined) {
    writeFileSync(join(out, SPEC_NAME), JSON.stringify(JSON.parse(readFileSync(spec, "utf8"))));
  }
}

function packCopies(out: string): void {
  for (const name of listFiles(out)) {
    if (!isCardPath(name)) {
      throw new Error(`${name} is not a name the board can serve`);
    }
    const source = readFileSync(join(out, name));
    const packed = gzipSync(source, { level: constants.Z_BEST_COMPRESSION });
    if (
      PACKED_EXTENSIONS.includes(posix.extname(name).toLowerCase()) &&
      packed.length < source.length
    ) {
      writeFileSync(join(out, `${name}${GZIP_SUFFIX}`), packed);
    }
  }
}

export function finishCard(out: string, spec: string | undefined): void {
  minifyPages(out);
  writeSpec(out, spec);
  packCopies(out);
}

export function cardConfig(options: CardOptions): UserConfig {
  const input = Object.fromEntries(
    Object.entries(options.pages).map(([name, page]) => [name, resolve(options.root, page)]),
  );
  const out = resolve(options.root, "dist");
  const board = options.board ?? process.env["MADOGUCHI_BOARD"] ?? "http://madoguchi.local";
  const plugin: Plugin = {
    name: "madoguchi-card",
    transformIndexHtml: { order: "pre", handler: (html: string) => withPolicy(fillCdn(html)) },
    closeBundle: { order: "post", handler: () => finishCard(out, options.spec) },
  };
  return {
    root: options.root,
    publicDir: false,
    plugins: [plugin],
    server: { proxy: { "/api": { target: board, changeOrigin: true } } },
    build: {
      outDir: out,
      emptyOutDir: true,
      assetsDir: "",
      modulePreload: { polyfill: false },
      rolldownOptions: {
        input,
        output: {
          entryFileNames: "[hash:8].js",
          chunkFileNames: "[hash:8].js",
          assetFileNames: "[hash:8][extname]",
          hashCharacters: "hex",
        },
      },
    },
  };
}
