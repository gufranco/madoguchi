import { faker } from "@faker-js/faker";
import { beforeEach, describe, expect, it } from "vitest";

import {
  apiError,
  describeFailure,
  failureStatus,
  readJson,
  readProblem,
  readText,
  requestJson,
  sendCommand,
} from "./api.ts";

function reply(status: number, body: string, type: string): Response {
  return new Response(status === 204 ? null : body, { status, headers: { "Content-Type": type } });
}

const isName = (value: unknown): value is { readonly name: string } =>
  typeof value === "object" &&
  value !== null &&
  typeof (value as { name?: unknown }).name === "string";

describe("api", () => {
  beforeEach(() => {
    faker.seed(20261028);
  });

  it("returns a body that passes its check", async () => {
    // Arrange
    const name = faker.word.noun();
    const fetcher = async () => reply(200, JSON.stringify({ name }), "application/json");

    // Act
    const value = await requestJson(fetcher, "GET", "/api/x", isName);

    // Assert
    expect(value).toEqual({ name });
  });

  it("returns null for 204 No Content", async () => {
    // Arrange
    const fetcher = async () => reply(204, "", "application/json");

    // Act
    const value = await requestJson(fetcher, "DELETE", "/api/x/1", isName);

    // Assert
    expect(value).toBeNull();
  });

  it("names the problem detail of a refused call", async () => {
    // Arrange
    const detail = faker.lorem.sentence();
    const body = JSON.stringify({ title: "Bad Request", status: 400, detail });
    const fetcher = async () => reply(400, body, "application/problem+json");

    // Act
    const failure = requestJson(fetcher, "POST", "/api/items", isName);

    // Assert
    await expect(failure).rejects.toMatchObject({
      message: `POST /api/items answered 400: ${detail}`,
      cause: { status: 400 },
    });
  });

  it("refuses a body that fails its check", async () => {
    // Arrange
    const fetcher = async () =>
      reply(200, JSON.stringify({ id: faker.number.int() }), "application/json");

    // Act
    const failure = requestJson(fetcher, "GET", "/api/x", isName);

    // Assert
    await expect(failure).rejects.toThrow("GET /api/x answered a body of the wrong shape");
  });

  it("reads a problem only from a problem document", async () => {
    // Arrange
    const plain = reply(500, "oops", "text/plain");

    // Act
    const problem = await readProblem(plain);

    // Assert
    expect(problem).toBeUndefined();
    expect(await readProblem(reply(503, "{", "application/problem+json"))).toBeUndefined();
    expect(await readProblem(reply(503, '"down"', "application/problem+json"))).toBeUndefined();
    expect(
      await readProblem(reply(503, '{"status":503,"title":7}', "application/problem+json")),
    ).toEqual({
      status: 503,
    });
    expect(await readProblem(new Response("x", { status: 500 }))).toBeUndefined();
    expect(
      await readProblem(reply(503, '{"title":"No card"}', "application/problem+json")),
    ).toEqual({
      title: "No card",
    });
  });

  it("describes a failure without a problem", () => {
    // Arrange
    const status = 401;

    // Act
    const text = describeFailure("DELETE", "/api/items/2", status, undefined);

    // Assert
    expect(text).toBe("DELETE /api/items/2 answered 401");
    expect(describeFailure("GET", "/", 503, { title: "No card" })).toBe(
      "GET / answered 503: No card",
    );
  });

  it("reads the status back from a failure", () => {
    // Arrange
    const failure = apiError(409, "conflict");

    // Act
    const status = failureStatus(failure);

    // Assert
    expect(status).toBe(409);
    expect(failureStatus(new Error("plain"))).toBeUndefined();
    expect(failureStatus("text")).toBeUndefined();
  });

  it("requires a body where one is expected", async () => {
    // Arrange
    const name = faker.word.noun();
    const full = async () => reply(200, JSON.stringify({ name }), "application/json");
    const empty = async () => reply(204, "", "application/json");

    // Act
    const value = await readJson(full, "GET", "/api/x", isName);

    // Assert
    expect(value).toEqual({ name });
    await expect(readJson(empty, "GET", "/api/x", isName)).rejects.toThrow(
      "GET /api/x answered no body",
    );
  });

  it("sends a command and accepts any success", async () => {
    // Arrange
    const done = async () => reply(204, "", "application/json");
    const refused = async () => reply(409, '{"title":"Conflict"}', "application/problem+json");

    // Act
    const sent = sendCommand(done, "DELETE", "/api/items/1");

    // Assert
    await expect(sent).resolves.toBeUndefined();
    await expect(sendCommand(refused, "DELETE", "/api/items/1")).rejects.toThrow(
      "DELETE /api/items/1 answered 409: Conflict",
    );
  });

  it("reads a text body and names a refusal", async () => {
    // Arrange
    const line = `{"level":30,"msg":"${faker.lorem.word()}"}\n`;
    const partial = async () => new Response(line, { status: 206 });
    const refused = async () => reply(401, '{"title":"Unauthorized"}', "application/problem+json");

    // Act
    const text = await readText(partial, "GET", "/api/log");

    // Assert
    expect(text).toBe(line);
    await expect(readText(refused, "GET", "/api/log")).rejects.toThrow(
      "GET /api/log answered 401: Unauthorized",
    );
  });
});
