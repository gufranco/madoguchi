import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import test from "node:test";
import vm from "node:vm";

import { fakeElement, seeded, settle, submit, text, word } from "./fake_dom.mjs";

const source = readFileSync(new URL("../../examples/RestFiles/www/app.js", import.meta.url), "utf8");
const problemType = "application/problem+json";
const helloDigest = "sha-256=:LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmCQ=:";

const syncSubtle = {
  digest: async (algorithm, data) => Uint8Array.from(createHash("sha256").update(new Uint8Array(data)).digest()).buffer,
};

function headers(type) {
  return { get: (name) => (name === "Content-Type" ? type : null) };
}

function fakeUploads(answer) {
  let sent = [];
  function FakeRequest() {
    const upload = fakeElement("upload");
    const events = fakeElement("request");
    let request = { headers: {} };
    return {
      upload,
      get status() {
        return answer.status;
      },
      get responseText() {
        return answer.text;
      },
      open: (method, path) => {
        request = { ...request, method, path };
      },
      setRequestHeader: (name, value) => {
        request = { ...request, headers: { ...request.headers, [name]: value } };
      },
      addEventListener: (type, handler) => events.addEventListener(type, handler),
      getResponseHeader: (name) => (name === "Content-Type" ? answer.type : null),
      send: (body) => {
        sent = [...sent, { ...request, body }];
        upload.listener("progress")({ lengthComputable: true, loaded: body.size, total: body.size });
        events.listener("load")();
      },
    };
  }
  return { FakeRequest, sent: () => sent };
}

function page(listings, options) {
  const settings = { file: null, answer: { status: 201, type: "", text: "" }, ...options };
  let requests = [];
  let queue = [...listings];
  const uploads = fakeUploads(settings.answer);
  const elements = {
    status: fakeElement("p"),
    files: fakeElement("ul"),
    file: { files: settings.file ? [settings.file] : [] },
    progress: { max: 1, value: 0 },
    upload: fakeElement("form"),
  };
  const fetch = async (path, init) => {
    requests = [...requests, { path, ...init }];
    const [next, ...rest] = queue;
    queue = rest;
    return {
      ok: next.status < 400,
      status: next.status,
      headers: headers(next.type ?? "application/json"),
      json: async () => next.body,
    };
  };
  const document = {
    getElementById: (id) => elements[id],
    createElement: (tag) => fakeElement(tag),
  };
  vm.runInNewContext(source, {
    document,
    fetch,
    btoa,
    crypto: { subtle: syncSubtle },
    XMLHttpRequest: uploads.FakeRequest,
    Uint8Array,
    Array,
    String,
    JSON,
    Error,
    Promise,
  });
  return { elements, requests: () => requests, uploads: uploads.sent };
}

function upload(name, bytes) {
  return { name, size: bytes.length, arrayBuffer: async () => bytes.buffer };
}

test("lists files and skips folders", async () => {
  // Arrange
  const name = `${word(seeded(11)).slice(0, 6).toUpperCase()}.TXT`;
  const listing = [
    { name, size: 42, directory: false },
    { name: "GZ", size: 0, directory: true },
  ];

  // Act
  const view = page([{ status: 200, body: listing }], {});
  await settle();

  // Assert
  const rows = view.elements.files.nodes();
  const [link, size, remove] = rows[0].nodes();
  assert.equal(rows.length, 1);
  assert.equal(link.attribute("href"), `/api/files/${name}?download=1`);
  assert.equal(text(size), "42 bytes");
  assert.equal(text(remove), `Delete ${name}`);
  assert.equal(text(view.elements.status), "Done.");
});

test("uploads under the upper-case name with its digest", async () => {
  // Arrange
  const bytes = new TextEncoder().encode("hello");
  const name = `${word(seeded(12)).slice(0, 5)}.txt`;
  const view = page(
    [
      { status: 200, body: [] },
      { status: 200, body: [] },
    ],
    { file: upload(name, bytes) },
  );
  await settle();

  // Act
  submit(view.elements.upload);
  await settle();

  // Assert
  const [sent] = view.uploads();
  assert.equal(sent.method, "PUT");
  assert.equal(sent.path, `/api/files/${name.toUpperCase()}`);
  assert.equal(sent.headers["Content-Type"], "application/octet-stream");
  assert.equal(sent.headers["Repr-Digest"], helloDigest);
  assert.equal(view.elements.progress.value, bytes.length);
  assert.equal(view.requests().length, 2);
  assert.equal(text(view.elements.status), "Done.");
});

test("refuses a name the card cannot store without a request", async () => {
  // Arrange
  const bytes = new TextEncoder().encode(word(seeded(13)));
  const view = page([{ status: 200, body: [] }], { file: upload("longername.text", bytes) });
  await settle();

  // Act
  submit(view.elements.upload);
  await settle();

  // Assert
  assert.equal(view.uploads().length, 0);
  assert.equal(text(view.elements.status), "Choose a file with an 8.3 name, such as LOG.TXT.");
});

test("shows the problem detail of a refused upload", async () => {
  // Arrange
  const bytes = new TextEncoder().encode(word(seeded(14)));
  const problem = JSON.stringify({ title: "Too large", detail: "the card is full" });
  const answer = { status: 413, type: problemType, text: problem };
  const view = page([{ status: 200, body: [] }], { file: upload("DATA.BIN", bytes), answer });
  await settle();

  // Act
  submit(view.elements.upload);
  await settle();

  // Assert
  assert.equal(text(view.elements.status), "PUT /api/files/DATA.BIN answered 413: the card is full");
});

test("deletes a file and lists again", async () => {
  // Arrange
  const name = `${word(seeded(15)).slice(0, 4).toUpperCase()}.LOG`;
  const view = page(
    [
      { status: 200, body: [{ name, size: 1, directory: false }] },
      { status: 204, body: null },
      { status: 200, body: [] },
    ],
    {},
  );
  await settle();
  const remove = view.elements.files.nodes()[0].nodes()[2];

  // Act
  remove.listener("click")();
  await settle();

  // Assert
  assert.equal(view.requests()[1].method, "DELETE");
  assert.equal(view.requests()[1].path, `/api/files/${name}`);
  assert.equal(view.elements.files.nodes().length, 0);
  assert.equal(text(view.elements.status), "Done.");
});

test("names the problem of a refused listing", async () => {
  // Arrange
  const listing = { status: 503, type: problemType, body: { title: "No card" } };

  // Act
  const view = page([listing], {});
  await settle();

  // Assert
  assert.equal(text(view.elements.status), "GET /api/files answered 503: No card");
});
