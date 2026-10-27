const FIRST_DELAY_MS = 400;
const LONGEST_DELAY_MS = 10000;

const toggle = document.getElementById("toggle");
const label = document.getElementById("label");
const status = document.getElementById("status");
let delay = FIRST_DELAY_MS;

function show(lit) {
  toggle.setAttribute("aria-pressed", lit ? "true" : "false");
  label.textContent = lit ? "Switch the LED off" : "Switch the LED on";
  status.textContent = lit ? "The LED is lit." : "The LED is dark.";
}

async function ledState(response) {
  if (!response.ok) {
    throw new Error(`the board answered ${response.status}`);
  }
  const body = await response.json();
  return body.led === 1;
}

async function poll() {
  try {
    show(await ledState(await fetch("/led", { cache: "no-store" })));
    delay = FIRST_DELAY_MS;
  } catch (error) {
    status.textContent = `No answer from the board: ${error.message}`;
    delay = Math.min(delay * 2, LONGEST_DELAY_MS);
  }
  window.setTimeout(poll, delay);
}

toggle.addEventListener("click", async () => {
  const lit = toggle.getAttribute("aria-pressed") === "true";
  try {
    show(await ledState(await fetch("/led", { method: "POST", body: lit ? "0" : "1" })));
  } catch (error) {
    status.textContent = `The LED did not change: ${error.message}`;
  }
});

poll();
