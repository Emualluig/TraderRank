import { GLOBAL_MARKET_STATE, WSMessage } from './core';
import { PanelBook } from './Panels/PanelBook';
import { PanelManager } from './Panels/PanelManager';
import { PanelText } from './Panels/PanelText';
import './style.css';
import './tw.css';

const WS_ENDPOINT = "http://localhost:8765";
const WS = new WebSocket(WS_ENDPOINT);
WS.onopen = () => {
  console.log("Connected to server");
}
WS.onerror = () => {
  console.log("On error!");
}
WS.close = () => {
  console.log("Disconnected from server!");
}
WS.onmessage = (event) => {
  try {
    const json = JSON.parse(event.data);
    if (json?.["type_"] === undefined) {
      throw new Error("Data was not matching the expected format.");
    }
    const message = json as WSMessage;
    if (message.type_ === "sync") {
      GLOBAL_MARKET_STATE.has_received_sync_message = true;
      GLOBAL_MARKET_STATE.sync_message = message;
      for (const panel of PANEL_MANAGER.ValuesPanel()) {
        panel.onSync(message);
      }
      console.log("ID:", message.client_id);
    } else if (message.type_ === "update") {
      GLOBAL_MARKET_STATE.update_message = message;
      for (const panel of PANEL_MANAGER.ValuesPanel()) {
        panel.onUpdate(message);
      }
    } else {
      throw new Error(`Received unknown message type: "${(message as any)?.type}".`);
    }
  } catch (err) {
    console.log("Failed decoding WS data to JSON.");
    console.log(err);
    console.log(event.data);
    return;
  }
}

const WORKSPACE = document.getElementById("workspace");
if (!WORKSPACE) {
  throw new Error("Invariant: Couldn't find `workspace`.");
}
const PANEL_MANAGER = new PanelManager(WORKSPACE);

// Add actions to the buttons
const ADD_ELEMENT_BUTTON = document.getElementById("add-element-button")
if (!ADD_ELEMENT_BUTTON) {
  throw new Error("Invariant: Couldn't find add button.");
}
ADD_ELEMENT_BUTTON.addEventListener("click", () => {
  const _ = new PanelText(PANEL_MANAGER);
});

const ADD_BOOK_BUTTON = document.getElementById("add-panel-book")
if (!ADD_BOOK_BUTTON) {
  throw new Error("Invariant: Couldn't find add book button.");
}
ADD_BOOK_BUTTON.addEventListener("click", () => {
  const _ = new PanelBook(PANEL_MANAGER);
});

const ADD_CHART_BUTTON = document.getElementById("add-panel-chart")
if (!ADD_CHART_BUTTON) {
  throw new Error("Invariant: Couldn't find add chart button.");
}
ADD_CHART_BUTTON.addEventListener("click", () => {
  // TODO: implement
  throw new Error("NOT IMPLEMENTED!");
  // const _ = new PanelChart(PANEL_MANAGER);
});

const ADD_CLOB_BUTTON = document.getElementById("add-panel-clob")
if (!ADD_CLOB_BUTTON) {
  throw new Error("Invariant: Couldn't find add clob button.");
}
ADD_CLOB_BUTTON.addEventListener("click", () => {
  // TODO: implement
  throw new Error("NOT IMPLEMENTED!");
  // const _ = new PanelCLOB(PANEL_MANAGER);
});
