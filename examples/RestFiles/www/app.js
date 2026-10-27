"use strict";
const statusLine = document.getElementById("status");
const list = document.getElementById("files");
const picker = document.getElementById("file");
const progress = document.getElementById("progress");
const shortName = /^[A-Za-z0-9_~-]{1,8}(\.[A-Za-z0-9_~-]{1,3})?$/;

function say(text) {
  statusLine.replaceChildren(text);
}

function fileUrl(name) {
  return "/api/files/" + encodeURIComponent(name);
}

function describe(method, path, status, problem) {
  const detail = problem && (problem.detail || problem.title);
  return method + " " + path + " answered " + status + (detail ? ": " + detail : "");
}

async function readProblem(response) {
  const type = response.headers.get("Content-Type") || "";
  return type.startsWith("application/problem+json") ? response.json() : null;
}

async function call(method, path) {
  const response = await fetch(path, { method });
  if (!response.ok) {
    throw new Error(describe(method, path, response.status, await readProblem(response)));
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

function row(file) {
  const entry = document.createElement("li");
  const link = document.createElement("a");
  link.setAttribute("href", fileUrl(file.name) + "?download=1");
  link.append(file.name);
  const size = document.createElement("span");
  size.append(file.size + " bytes");
  entry.append(link, size, button("Delete " + file.name, () => call("DELETE", fileUrl(file.name))));
  return entry;
}

async function refresh() {
  const files = await call("GET", "/api/files");
  list.replaceChildren(...files.filter((file) => !file.directory).map(row));
}

function toBase64(bytes) {
  return btoa(Array.from(bytes, (value) => String.fromCharCode(value)).join(""));
}

async function digestOf(file) {
  const subtle = globalThis.crypto && globalThis.crypto.subtle;
  if (!subtle) {
    return null;
  }
  const hash = await subtle.digest("SHA-256", await file.arrayBuffer());
  return "sha-256=:" + toBase64(new Uint8Array(hash)) + ":";
}

function send(file, digest) {
  return new Promise((resolve, reject) => {
    const request = new XMLHttpRequest();
    const path = fileUrl(file.name.toUpperCase());
    request.open("PUT", path);
    request.setRequestHeader("Content-Type", "application/octet-stream");
    if (digest) {
      request.setRequestHeader("Repr-Digest", digest);
    }
    request.upload.addEventListener("progress", (event) => {
      progress.max = event.lengthComputable ? event.total : 1;
      progress.value = event.lengthComputable ? event.loaded : 0;
    });
    request.addEventListener("load", () => {
      const ok = request.status === 201 || request.status === 204;
      const type = request.getResponseHeader("Content-Type") || "";
      const typed = type.startsWith("application/problem+json") && request.responseText;
      const problem = !ok && typed ? JSON.parse(request.responseText) : null;
      return ok ? resolve() : reject(new Error(describe("PUT", path, request.status, problem)));
    });
    request.addEventListener("error", () => reject(new Error("PUT " + path + " did not finish")));
    request.send(file);
  });
}

async function run(action) {
  try {
    await action();
    await refresh();
    say("Done.");
  } catch (error) {
    say(error.message);
  }
}

document.getElementById("upload").addEventListener("submit", (event) => {
  event.preventDefault();
  const file = picker.files[0];
  if (!file || !shortName.test(file.name)) {
    say("Choose a file with an 8.3 name, such as LOG.TXT.");
    return;
  }
  run(async () => send(file, await digestOf(file)));
});

run(async () => {});
