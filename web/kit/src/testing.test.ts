import { describe, expect, it } from "vitest";

import { answer, fakeBoard, problem } from "./testing.ts";

const PAGE =
  '<!doctype html><html><body><form><input id="name"></form><p role="status"></p></body></html>';

describe("fake board", () => {
  it("records each call and plays the answers in order", async () => {
    // Arrange
    const board = fakeBoard(PAGE, [answer(200, { a: 1 }), answer(204, null)]);

    // Act
    const first = await board.host.fetch("/api/a", {
      method: "POST",
      body: '{"x":1}',
      headers: { "Content-Type": "application/json" },
    });

    // Assert
    expect(await first.json()).toEqual({ a: 1 });
    expect((await board.host.fetch("/api/b", {})).status).toBe(204);
    expect((await board.host.fetch("/api/c", {})).status).toBe(500);
    expect(board.calls()).toEqual([
      {
        path: "/api/a",
        method: "POST",
        body: '{"x":1}',
        headers: { "content-type": "application/json" },
      },
      { path: "/api/b", method: "GET", body: undefined, headers: {} },
      { path: "/api/c", method: "GET", body: undefined, headers: {} },
    ]);
  });

  it("reads text and inputs from the page", () => {
    // Arrange
    const board = fakeBoard(PAGE, []);

    // Act
    board.input("name").value = "lamp";

    // Assert
    expect(board.input("name").value).toBe("lamp");
    expect(board.text('[role="status"]')).toBe("");
    expect(board.text("#missing")).toBe("");
    expect(() => board.input("missing")).toThrow("no #missing");
    expect(board.host.matchMedia("(prefers-color-scheme: dark)").matches).toBe(false);
  });

  it("builds a problem document", async () => {
    // Arrange
    const detail = "the card is full";

    // Act
    const response = problem(507, detail);

    // Assert
    expect(response.headers.get("Content-Type")).toBe("application/problem+json");
    expect(await response.json()).toEqual({ title: "Error", status: 507, detail });
  });

  it("submits the first form", () => {
    // Arrange
    const board = fakeBoard(PAGE, []);
    let submitted = false;
    board.window.document.querySelector("form")?.addEventListener("submit", (event) => {
      event.preventDefault();
      submitted = true;
    });

    // Act
    board.submit("form");

    // Assert
    expect(submitted).toBe(true);
  });

  it("clicks an element", () => {
    // Arrange
    const board = fakeBoard('<button id="go" type="button">go</button>', []);
    let clicks = 0;
    board.window.document.querySelector("#go")?.addEventListener("click", () => {
      clicks += 1;
    });

    // Act
    board.click("#go");

    // Assert
    expect(clicks).toBe(1);
  });
});
