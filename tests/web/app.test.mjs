import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import vm from "node:vm";

import { fakeElement, seeded, settle, submit, text, word } from "./fake_dom.mjs";

const source = readFileSync(new URL("../../examples/RestApi/www/app.js", import.meta.url), "utf8");

function page(responses, itemName = "", passphrase = "") {
  let requests = [];
  let queue = [...responses];
  const elements = {
    status: fakeElement("p"),
    items: fakeElement("ul"),
    login: fakeElement("form", {
      elements: { username: { value: "admin" }, password: { value: passphrase } },
    }),
    add: fakeElement("form"),
    name: { value: itemName },
    value: { value: "0" },
    on: { checked: false },
    "show-log": fakeElement("button"),
    log: fakeElement("pre"),
  };
  const fetch = async (path, options) => {
    requests = [...requests, { path, ...options }];
    const [next, ...rest] = queue;
    queue = rest;
    return {
      ok: next.status < 400,
      status: next.status,
      json: async () => next.body,
      text: async () => next.body,
    };
  };
  const document = {
    getElementById: (id) => elements[id],
    createElement: (tag) => fakeElement(tag),
  };
  vm.runInNewContext(source, { document, fetch, JSON, Number, Error });
  return { elements, requests: () => requests };
}

test("lists the items on load", async () => {
  // Arrange
  const name = word(seeded(1));
  const view = page([{ status: 200, body: [{ id: 1, name, value: 3, on: true }] }]);

  // Act
  await settle();

  // Assert
  const rows = view.elements.items.nodes();
  assert.equal(rows.length, 1);
  assert.equal(text(rows[0].nodes()[0]), `${name}: 3, on`);
  assert.equal(text(view.elements.status), "Sign in to change items.");
});

test("signs in and sends the token on writes", async () => {
  // Arrange
  const random = seeded(2);
  const token = word(random);
  const passphrase = word(random) + word(random);
  const view = page(
    [
      { status: 200, body: [] },
      { status: 200, body: { access_token: token, token_type: "Bearer", expires_in: 900 } },
      { status: 200, body: [] },
      { status: 201, body: { id: 1, name: "x", value: 0, on: false } },
      { status: 200, body: [] },
    ],
    word(random),
    passphrase,
  );
  await settle();
  submit(view.elements.login);
  await settle();

  // Act
  submit(view.elements.add);
  await settle();

  // Assert
  const [, login, , create] = view.requests();
  assert.equal(login.path, "/api/auth/login");
  assert.deepEqual(JSON.parse(login.body), { username: "admin", password: passphrase });
  assert.equal(view.elements.login.resets(), 1);
  assert.equal(create.method, "POST");
  assert.equal(create.headers.Authorization, `Bearer ${token}`);
  assert.equal(create.headers["Content-Type"], "application/json");
  assert.equal(text(view.elements.status), "Done.");
});

test("patches the item a toggle names", async () => {
  // Arrange
  const name = word(seeded(3));
  const view = page([
    { status: 200, body: [{ id: 4, name, value: 1, on: true }] },
    { status: 200, body: { id: 4, name, value: 1, on: false } },
    { status: 200, body: [] },
  ]);
  await settle();
  const toggle = view.elements.items.nodes()[0].nodes()[1];

  // Act
  toggle.listener("click")();
  await settle();

  // Assert
  const patch = view.requests()[1];
  assert.equal(toggle.attribute("type"), "button");
  assert.equal(text(toggle), `Toggle ${name}`);
  assert.equal(patch.method, "PATCH");
  assert.equal(patch.path, "/api/items/4");
  assert.deepEqual(JSON.parse(patch.body), { on: false });
});

test("shows the status of a refused call", async () => {
  // Arrange
  const view = page([
    { status: 200, body: [{ id: 2, name: word(seeded(4)), value: 1, on: true }] },
    { status: 401, body: null },
  ]);
  await settle();
  const remove = view.elements.items.nodes()[0].nodes()[2];

  // Act
  remove.listener("click")();
  await settle();

  // Assert
  assert.equal(view.requests()[1].method, "DELETE");
  assert.equal(view.requests()[1].headers.Authorization, undefined);
  assert.equal(text(view.elements.status), "DELETE /api/items/2 answered 401");
});

test("shows the end of the log with the token", async () => {
  // Arrange
  const random = seeded(5);
  const token = word(random);
  const line = '{"level":30,"msg":"request completed"}\n';
  const view = page(
    [
      { status: 200, body: [] },
      { status: 200, body: { access_token: token, token_type: "Bearer", expires_in: 900 } },
      { status: 200, body: [] },
      { status: 206, body: line },
      { status: 200, body: [] },
    ],
    "",
    word(random),
  );
  await settle();
  submit(view.elements.login);
  await settle();

  // Act
  view.elements["show-log"].listener("click")();
  await settle();

  // Assert
  const read = view.requests()[3];
  assert.equal(read.path, "/api/log");
  assert.equal(read.headers.Range, "bytes=-4096");
  assert.equal(read.headers.Authorization, `Bearer ${token}`);
  assert.equal(text(view.elements.log), line);
});
