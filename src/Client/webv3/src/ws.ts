import { useGlobalStore } from "./store";
import type { Message } from "./types";

const WS_ENDPOINT = "http://localhost:8765";
export const WS = new WebSocket(WS_ENDPOINT);
WS.onopen = () => {
  console.log("Connected to server");
};
WS.onerror = () => {
  console.log("On error!");
};
WS.close = () => {
  console.log("Disconnected from server!");
};
WS.onmessage = (event) => {
  const state = useGlobalStore.getState();
  try {
    const msg = JSON.parse(event.data);
    if (msg?.["type_"] === undefined) {
      throw new Error("Didn't receive correctly formatted message.");
    }
    const message = msg as Message;
    if (message.type_ === "login_request") {
      state.processMessageLoginRequest(message);
    } else if (message.type_ === "login_response") {
      state.processMessageLoginResponse(message);
    } else if (message.type_ === "simulation_load") {
      state.processMessageSimulationLoad(message);
    } else if (message.type_ === "simulation_update") {
      state.processMessageSimulationUpdate(message);
    } else if (message.type_ === "market_update") {
      state.processMessageMarketUpdate(message);
    } else if (message.type_ === "chat_message_received") {
      state.processMessageChatMessageReceived(message);
    } else if (message.type_ === "new_user_connected") {
      state.processMessageNewUserConnected(message);
    } else {
      throw new Error("Unknown message type received.");
    }
  } catch (e) {
    console.log(e);
  }
};

export const sendMessage = () => {
  throw new Error("Not implemented!");
  if (WS.readyState === WebSocket.OPEN) {
    // WS.send(data);
  }
};
