import interact from 'interactjs';
import './style.css';
import './tw.css';
import { WindowClob, WindowDEV, WindowElement, WindowFrame } from './window';

const LABEL_ID = `#simulation-state-label`;
const WS = new WebSocket("http://localhost:8765");
WS.onopen = () => {
  const label = document.querySelector(LABEL_ID)
  if (label !== null) {
    label.innerHTML = `<span class="text-green-600 font-bold">ACTIVE</span>`;
  }
  console.log("Connected to server");
}
WS.onerror = () => {
  const label = document.querySelector(LABEL_ID)
  if (label !== null) {
    label.innerHTML = `<span class="text-red-900 font-bold">ERROR</span>`;
  }
  console.log("On error!");
}
WS.close = () => {
  const label = document.querySelector(LABEL_ID)
  if (label !== null) {
    label.innerHTML = `<span class="text-amber-600 font-bold">CLOSED</span>`;
  }
  console.log("Disconnected from server!");
}
let MARKET_STATE: object|null = null;
WS.onmessage = (event) => {
  try {
    const json = JSON.parse(event.data);
    MARKET_STATE = json;
  } catch (err) {
    console.log("Failed decoding WS data to JSON.");
    console.log(err);
    console.log(event.data);
    return;
  }
  for (const window of WINDOW_FRAME_MAP.values()) {
    window.Notify(MARKET_STATE);
  }
}


const WORKSPACE = document.getElementById("workspace");
if (!WORKSPACE) {
  throw new Error("Invariant: Couldn't find `workspace`.");
}
const WINDOW_MAP = new Map<string, WindowElement>();
const WINDOW_FRAME_MAP = new Map<string, WindowFrame>();

// Add actions to the buttons
const ADD_ELEMENT_BUTTON = document.getElementById("add-element-button")
if (!ADD_ELEMENT_BUTTON) {
  throw new Error("Invariant: Couldn't find add button.");
}
ADD_ELEMENT_BUTTON.addEventListener("click", () => {
  const _ = new WindowDEV(WORKSPACE, WINDOW_MAP, WINDOW_FRAME_MAP);
});

const ADD_CLOB_BUTTON = document.getElementById("add-window-clob")
if (!ADD_CLOB_BUTTON) {
  throw new Error("Invariant: Couldn't find add Clob button.");
}
ADD_CLOB_BUTTON.addEventListener("click", () => {
  const _ = new WindowClob(WORKSPACE, WINDOW_MAP, WINDOW_FRAME_MAP);
});
