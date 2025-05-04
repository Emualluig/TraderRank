import { GLOBAL_STATE, WSMessage, WSMessageSync, WSMessageUpdate } from './core';
import './style.css';
import './tw.css';

interface PanelEvents {
  sync: (message: WSMessageSync) => {};
  update: (message: WSMessageUpdate) => {};
}

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
    if (json?.["type"] === undefined) {
      throw new Error("Data was not matching the expected format.");
    }
    const message = json as WSMessage;
    if (message.type_ === "sync") {
      GLOBAL_STATE.has_received_sync_message = true;
      GLOBAL_STATE.sync_message = message;
    } else if (message.type_ === "update") {
      GLOBAL_STATE.update_message = message;
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
