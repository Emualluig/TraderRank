import type { Message } from './types';

const WS_ENDPOINT = "http://localhost:8765";
const WS = new WebSocket(WS_ENDPOINT);
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
    try {
        const msg = JSON.parse(event.data);
        if (msg?.["type_"] === undefined) {
            throw new Error("Didn't receive correctly formatted message.");
        }
        const message = msg as Message;
        if (message.type_ === "login_request") {
            throw new Error("Client shouldn't receive login requests.");
        } else if (message.type_ === "login_response") {
            console.log(message);
        } else if (message.type_ === "simulation_load") {
            console.log(message);
        } else if (message.type_ === "simulation_update") {
            console.log(message);
        } else if (message.type_ === "market_update") {
            console.log(message);
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
}
