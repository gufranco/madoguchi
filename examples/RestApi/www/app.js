"use strict";
let token = "";
const statusLine = document.getElementById("status");
const list = document.getElementById("items");

function say(text) {
  statusLine.replaceChildren(text);
}

function headersFor(hasBody) {
  return {
    ...(hasBody ? { "Content-Type": "application/json" } : {}),
    ...(token ? { Authorization: "Bearer " + token } : {}),
  };
}

async function call(method, path, body) {
  const hasBody = body !== undefined;
  const response = await fetch(path, {
    method,
    headers: headersFor(hasBody),
    body: hasBody ? JSON.stringify(body) : undefined,
  });
  if (!response.ok) {
    throw new Error(method + " " + path + " answered " + response.status);
  }
  return response.status === 204 ? null : response.json();
}

function button(label, action) {
  const element = document.createElement("button");
  element.setAttribute("type", "button");
  element.append(label);
  element.addEventListener("click", () => run(action));
  return element;
}

function row(item) {
  const entry = document.createElement("li");
  const label = document.createElement("span");
  label.append(item.name + ": " + item.value + (item.on ? ", on" : ", off"));
  entry.append(
    label,
    button("Toggle " + item.name, () => call("PATCH", "/api/items/" + item.id, { on: !item.on })),
    button("Delete " + item.name, () => call("DELETE", "/api/items/" + item.id)),
  );
  return entry;
}

async function refresh() {
  const items = await call("GET", "/api/items");
  list.replaceChildren(...items.map(row));
}

async function run(action) {
  try {
    await action();
    await refresh();
    say(token ? "Done." : "Sign in to change items.");
  } catch (error) {
    say(error.message);
  }
}

document.getElementById("login").addEventListener("submit", (event) => {
  event.preventDefault();
  const form = event.target;
  const credentials = {
    username: form.elements.username.value,
    password: form.elements.password.value,
  };
  form.reset();
  run(async () => {
    const answer = await call("POST", "/api/auth/login", credentials);
    token = answer.access_token;
  });
});

document.getElementById("add").addEventListener("submit", (event) => {
  event.preventDefault();
  run(() =>
    call("POST", "/api/items", {
      name: document.getElementById("name").value,
      value: Number(document.getElementById("value").value),
      on: document.getElementById("on").checked,
    }),
  );
});

async function showLog() {
  const response = await fetch("/api/log", {
    headers: { Range: "bytes=-4096", ...(token ? { Authorization: "Bearer " + token } : {}) },
  });
  if (!response.ok) {
    throw new Error("GET /api/log answered " + response.status);
  }
  document.getElementById("log").replaceChildren(await response.text());
}

document.getElementById("show-log").addEventListener("click", () => run(showLog));

run(async () => {});
