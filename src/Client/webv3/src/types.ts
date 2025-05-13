export type Ticker = string;
export type UserID = number;
export type OrderID = number;
export type Username = string;

export interface LimitOrder {
  order_id: OrderID;
  price: number;
  user_id: UserID;
  volume: number;
}
export interface OrderBook {
  bids: LimitOrder[];
  asks: LimitOrder[];
}

export interface Transaction {
  tick: number;
  price: number;
  volume: number;
  seller_id: number;
  buyer_id: number;
}

export interface SecurityInfo {
  security_id: number;
  net_limit: number;
  gross_limit: number;
  max_trade_volume: number;
}

export interface News {
  tick: number;
  text: string;
}

export type SimulationState = "running" | "paused";
type MessageType =
  | "login_request"
  | "login_response"
  | "simulation_load"
  | "simulation_update"
  | "market_update"
  | "new_user_connected";
interface MessageBase {
  type_: MessageType;
}
export interface MessageLoginRequest extends MessageBase {
  type_: "login_request";
  username: Username;
}
export interface MessageLoginResponse extends MessageBase {
  type_: "login_response";
  client_id: UserID;
}
export interface MessageSimulationLoad extends MessageBase {
  type_: "simulation_load";
  simulation_state: SimulationState;
  tick: number;
  all_securities: Ticker[];
  tradeable_securities: Ticker[];
  security_info: Record<Ticker, SecurityInfo>;
  order_book_per_security: Record<Ticker, OrderBook>;
  transactions: Record<Ticker, Transaction[]>;
  user_id_to_username: Record<UserID, Username>;
  portfolio: Record<Ticker, number>;
  news: News[];
}
export interface MessageSimulationUpdate extends MessageBase {
  type_: "simulation_update";
  simulation_state: SimulationState;
  tick: number;
}
export interface MessageMarketUpdate extends MessageBase {
  type_: "market_update";
  tick: number;
  order_book_per_security: Record<Ticker, OrderBook>;
  portfolio: Record<Ticker, number>;
  new_transactions: Record<Ticker, Transaction[]>;
  new_news: News[];
}
export interface MessageNewUserConnected extends MessageBase {
  type_: "new_user_connected";
  user_id: UserID;
  username: Username;
}

type MessageMap = {
  MessageLoginRequest: MessageLoginRequest;
  MessageLoginResponse: MessageLoginResponse;
  MessageSimulationLoad: MessageSimulationLoad;
  MessageSimulationUpdate: MessageSimulationUpdate;
  MessageMarketUpdate: MessageMarketUpdate;
  MessageNewUserConnected: MessageNewUserConnected;
};

export type MessageProcessor = {
  [K in keyof MessageMap as `process${K}`]: (msg: MessageMap[K]) => void;
};

export type Message = MessageMap[keyof MessageMap];
