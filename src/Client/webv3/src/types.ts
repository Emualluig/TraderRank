type Ticker = string;
type UserID = number;
type OrderID = number;
type Username = string;

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

type SimulationState = "running"|"paused";
type MessageType = "login_request"|"login_response"|"simulation_load"|"simulation_update"|"market_update";
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
    step: number;
    securities: Ticker[];
    tradeable_securities: Ticker[];
    order_book_per_security: Record<Ticker, OrderBook>;
    portfolio: Record<Ticker, number>;
}
export interface MessageSimulationUpdate extends MessageBase {
    type_: "simulation_update";
    simulation_state: SimulationState;
    step: number;
}
export interface MessageMarketUpdate extends MessageBase {
    type_: "market_update";
    step: number;
    order_book_per_security: Record<Ticker, OrderBook>;
    portfolio: Record<Ticker, number>;
}
export type Message = MessageLoginRequest|MessageLoginResponse|
    MessageSimulationLoad|MessageSimulationUpdate|MessageMarketUpdate

    