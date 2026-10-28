import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { faker } from "@faker-js/faker";
import { answer, type FakeBoard, fakeBoard, problem } from "@madoguchi/kit/testing";
import { beforeEach, describe, expect, it } from "vitest";

import { type FilesHost, start } from "./app.ts";

const PAGE = readFileSync(resolve(import.meta.dirname, "../index.html"), "utf8");
const STATUS = '[role="status"]';
const HELLO_DIGEST = "sha-256=:LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmCQ=:";

interface Upload {
  readonly method: string;
  readonly path: string;
  readonly headers: Readonly<Record<string, string>>;
  readonly size: number;
}

interface Reply {
  readonly status: number;
  readonly type: string;
  readonly text: string;
  readonly fails?: boolean;
}

type Listener = (event: { lengthComputable: boolean; loaded: number; total: number }) => void;

function fakeRequests(reply: Reply): {
  readonly Request: unknown;
  readonly sent: () => readonly Upload[];
} {
  let sent: readonly Upload[] = [];
  function Request() {
    let listeners: Readonly<Record<string, () => void>> = {};
    let progress: Listener = () => undefined;
    let opened = { method: "", path: "" };
    let headers: Readonly<Record<string, string>> = {};
    return {
      upload: {
        addEventListener: (_: string, listener: Listener) => {
          progress = listener;
        },
      },
      status: reply.status,
      responseText: reply.text,
      open: (method: string, path: string) => {
        opened = { method, path };
      },
      setRequestHeader: (name: string, value: string) => {
        headers = { ...headers, [name.toLowerCase()]: value };
      },
      getResponseHeader: (name: string) => (name === "Content-Type" ? reply.type : null),
      addEventListener: (type: string, listener: () => void) => {
        listeners = { ...listeners, [type]: listener };
      },
      send: (body: Blob) => {
        sent = [...sent, { ...opened, headers, size: body.size }];
        progress({ lengthComputable: true, loaded: body.size, total: body.size });
        listeners[reply.fails === true ? "error" : "load"]?.();
      },
    };
  }
  return { Request, sent: () => sent };
}

const subtle = {
  digest: async (_: string, data: BufferSource) =>
    Uint8Array.from(
      createHash("sha256")
        .update(new Uint8Array(data as ArrayBuffer))
        .digest(),
    ).buffer,
};

function filesHost(board: FakeBoard, reply: Reply) {
  const requests = fakeRequests(reply);
  const host = {
    ...board.host,
    XMLHttpRequest: requests.Request as typeof XMLHttpRequest,
    crypto: { subtle },
  } satisfies FilesHost;
  return { host, sent: requests.sent };
}

function choose(board: FakeBoard, name: string, content: string): void {
  const file = new board.window.File([content], name);
  Object.defineProperty(board.input("file"), "files", { value: [file], configurable: true });
}

describe("RestFiles page", () => {
  beforeEach(() => {
    faker.seed(20261110);
  });

  it("lists files, skips folders and links each download", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [
      answer(200, [
        { name: "LOG.TXT", size: 42, directory: false },
        { name: "GZ", size: 0, directory: true },
      ]),
    ]);
    const { host } = filesHost(board, { status: 201, type: "", text: "" });

    // Act
    start(host);

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("1 file.");
    expect(board.text("#files li a")).toBe("LOG.TXT");
    expect(board.window.document.querySelector("#files li a")?.getAttribute("href")).toBe(
      "/api/files/LOG.TXT?download=1",
    );
    expect(board.text("#files li span")).toBe("42 bytes");
  });

  it("uploads under the chosen name with its digest and progress", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [answer(200, []), answer(200, [])]);
    const files = filesHost(board, { status: 201, type: "", text: "" });
    start(files.host);
    await expect.poll(() => board.text(STATUS)).toBe("0 files.");
    choose(board, "hello.txt", "hello");

    // Act
    board.submit("#upload");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("Uploaded hello.txt.");
    expect(files.sent()).toEqual([
      {
        method: "PUT",
        path: "/api/files/hello.txt",
        headers: { "content-type": "application/octet-stream", "repr-digest": HELLO_DIGEST },
        size: 5,
      },
    ]);
    expect(board.window.document.querySelector("#progress")?.getAttribute("value")).toBe("5");
  });

  it("refuses a name the card cannot store before uploading", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [answer(200, [])]);
    const files = filesHost(board, { status: 201, type: "", text: "" });
    start(files.host);
    await expect.poll(() => board.text(STATUS)).toBe("0 files.");
    choose(board, "a?b.txt", faker.lorem.word());

    // Act
    board.submit("#upload");

    // Assert
    await expect
      .poll(() => board.text(STATUS))
      .toBe('Choose a file named with 1 to 64 ASCII characters and none of " * / : < > ? \\ | %.');
    expect(files.sent()).toEqual([]);
  });

  it("shows the problem detail of a refused upload", async () => {
    // Arrange
    const detail = faker.lorem.sentence();
    const board = fakeBoard(PAGE, [answer(200, [])]);
    const body = JSON.stringify({ title: "Too large", status: 413, detail });
    const files = filesHost(board, { status: 413, type: "application/problem+json", text: body });
    start(files.host);
    await expect.poll(() => board.text(STATUS)).toBe("0 files.");
    choose(board, "DATA.BIN", faker.lorem.word());

    // Act
    board.submit("#upload");

    // Assert
    await expect
      .poll(() => board.text(STATUS))
      .toBe(`PUT /api/files/DATA.BIN answered 413: ${detail}`);
  });

  it("reports an upload that never finished", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [answer(200, [])]);
    const files = filesHost(board, { status: 0, type: "", text: "", fails: true });
    start(files.host);
    await expect.poll(() => board.text(STATUS)).toBe("0 files.");
    choose(board, "A.TXT", "x");

    // Act
    board.submit("#upload");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("PUT /api/files/A.TXT did not finish");
  });

  it("deletes a file and lists again", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [
      answer(200, [{ name: "OLD.LOG", size: 1, directory: false }]),
      answer(204, null),
      answer(200, []),
    ]);
    const { host } = filesHost(board, { status: 201, type: "", text: "" });
    start(host);
    await expect.poll(() => board.text(STATUS)).toBe("1 file.");

    // Act
    board.click("#files button");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("0 files.");
    expect(board.calls()[1]?.method).toBe("DELETE");
    expect(board.calls()[1]?.path).toBe("/api/files/OLD.LOG");
  });

  it("shows why the board refused the listing", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [problem(503, "the SD card is not available")]);
    const { host } = filesHost(board, { status: 201, type: "", text: "" });

    // Act
    start(host);

    // Assert
    await expect
      .poll(() => board.text(STATUS))
      .toBe("GET /api/files answered 503: the SD card is not available");
  });

  it("asks for a file when none is chosen", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [answer(200, [])]);
    const files = filesHost(board, { status: 201, type: "", text: "" });
    start(files.host);
    await expect.poll(() => board.text(STATUS)).toBe("0 files.");

    // Act
    board.submit("#upload");

    // Assert
    await expect
      .poll(() => board.text(STATUS))
      .toBe('Choose a file named with 1 to 64 ASCII characters and none of " * / : < > ? \\ | %.');
    expect(files.sent()).toEqual([]);
  });

  it("names a refused upload that carries no problem document", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [answer(200, [])]);
    const files = filesHost(board, { status: 500, type: "", text: "" });
    start(files.host);
    await expect.poll(() => board.text(STATUS)).toBe("0 files.");
    choose(board, "B.TXT", faker.lorem.word());

    // Act
    board.submit("#upload");

    // Assert
    await expect.poll(() => board.text(STATUS)).toBe("PUT /api/files/B.TXT answered 500");
  });
});
