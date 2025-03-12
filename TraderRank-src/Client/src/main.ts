import './style.css';
import './tw.css';
import { PanelManager } from './Panels/PanelManager';
import { PanelText } from './Panels/PanelText';
import { MarketState, convertToSimulationStep } from './MarketHelpers';
import { PanelChart } from './Panels/PanelChart';
import { PanelCLOB } from './Panels/PanelCLOB';

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
let MARKET_STATE: MarketState = {
  midpointHistory: [],
  timestamp: 0,
  transactionsMap: new Map(),
  currentDepths: {
    asks: new Map(),
    bids: new Map()
  },
  top_ask: NaN,
  top_bid: NaN
};
WS.onmessage = (event) => {
  try {
    const json = JSON.parse(event.data);
    const step = convertToSimulationStep(json);
    if (step.timestamp === 0) {
      MARKET_STATE = {
        timestamp: step.timestamp,
        transactionsMap: new Map([[step.timestamp, step.transactions]]),
        currentDepths: step.depths,
        midpointHistory: [{timestamp: step.timestamp, price: (step.top_ask + step.top_bid) / 2}],
        top_ask: step.top_ask,
        top_bid: step.top_bid,
      };
    } else {
      MARKET_STATE.timestamp = step.timestamp;
      MARKET_STATE.midpointHistory.push({ timestamp: step.timestamp, price: (step.top_ask + step.top_bid) / 2 });
      MARKET_STATE.transactionsMap.set(step.timestamp, step.transactions);
      MARKET_STATE.currentDepths = step.depths;
      MARKET_STATE.top_ask = step.top_ask;
      MARKET_STATE.top_bid = step.top_bid;
    }
  } catch (err) {
    console.log("Failed decoding WS data to JSON.");
    console.log(err);
    console.log(event.data);
    return;
  }
  if (MARKET_STATE !== null) {
    for (const panel of PANEL_MANAGER.ValuesPanel()) {
      panel.Notify(MARKET_STATE);
    }
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
  console.log("NOT IMPLEMENTED");
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


