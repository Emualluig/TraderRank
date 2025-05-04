export interface LimitOrder {
    order_id: number;   
    price: number;
    user_id: number;
    volume: number;
}
export interface OrderBook {
    bids: LimitOrder[]; 
    asks: LimitOrder[];
}

interface WSMessageType {
    type_: "sync"|"update";
}
export interface WSMessageSync extends WSMessageType {
    type_: "sync";
    client_id: number;
    securities: string[];
    security_to_id: Map<string, number>;
    tradable_securities_id: number[];
    order_book_per_security: Record<string, OrderBook>;
}
export interface WSMessageUpdate extends WSMessageType {
    type_: "update";
    step: number;
    cancelled_orders: Record<string, number[]>;
    fully_transacted_orders: Record<string, number[]>;
    order_book_per_security: Record<string, OrderBook>;
    partially_transacted_orders: Record<string, number[]>;
    portfolios: Array<number[]>;
    user_id_to_username_map: Record<number, string>;
}
export type WSMessage = WSMessageSync|WSMessageUpdate;

interface GLOBAL_MARKET_STATE_OBJ {
    has_received_sync_message: boolean;
    sync_message: WSMessageSync|null;
    update_message: WSMessageUpdate|null;
}
export const GLOBAL_MARKET_STATE: GLOBAL_MARKET_STATE_OBJ = {
    has_received_sync_message: false,
    sync_message: null,
    update_message: null
};