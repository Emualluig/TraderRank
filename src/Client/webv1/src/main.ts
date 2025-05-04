import './style.css';
import './tw.css';
import { PanelManager } from './Panels/PanelManager';
import { PanelText } from './Panels/PanelText';
import { PanelChart } from './Panels/PanelChart';
import { PanelCLOB } from './Panels/PanelCLOB';
import { PanelBook } from './Panels/PanelBook';
import { MarketUpdate } from './types';



const WORKSPACE = document.getElementById("workspace");
if (!WORKSPACE) {
  throw new Error("Invariant: Couldn't find `workspace`.");
}
const PANEL_MANAGER = new PanelManager(WORKSPACE);

const WS = new WebSocket("http://localhost:8765");
WS.onopen = () => {
  console.log("Connected to server");
}
WS.onerror = () => {
  console.log("On error!");
}
WS.close = () => {
  console.log("Disconnected from server!");
}
let CLIENT_ID: number|null = null;
WS.onmessage = (event) => {
  let market_update: MarketUpdate|null = null;
  try {
    const json = JSON.parse(event.data);
    if (json?.["client_id"] !== undefined) {
      CLIENT_ID = parseInt(String(json["client_id"]));
      console.log(`Has id: ${CLIENT_ID}`)
    }
  } catch (err) {
    console.log("Failed decoding WS data to JSON.");
    console.log(err);
    console.log(event.data);
    return;
  }
  if (market_update !== null && CLIENT_ID !== null) {

  }
}

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
  const _ = new PanelChart(PANEL_MANAGER);
});

const ADD_CLOB_BUTTON = document.getElementById("add-panel-clob")
if (!ADD_CLOB_BUTTON) {
  throw new Error("Invariant: Couldn't find add clob button.");
}
ADD_CLOB_BUTTON.addEventListener("click", () => {
  const _ = new PanelCLOB(PANEL_MANAGER);
});


