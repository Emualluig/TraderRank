export interface LimitOrder {
  order_id: number;
  price: number;
  user_id: number;
  volume: number;
}

export interface MarketUpdate {
  cancelled_orders: Record<string, number[]>;
  fully_transacted_orders: Record<string, number[]>;
  order_book_per_security: Record<string, { bids: LimitOrder[], asks: LimitOrder[] }>;
  partially_transacted_orders: Record<string, number[]>;
  portfolios: Array<number[]>;
  step: number;
  user_id_to_username_map: Record<number, string>;
}
