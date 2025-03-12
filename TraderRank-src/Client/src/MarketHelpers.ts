export type Depth = Map<number, number>;
export interface MarketState {
  timestamp: number;
  transactionsMap: Map<number, Transaction[]>;
  midpointHistory: Array<{ timestamp: number; price: number; }>
  currentDepths: {
    bids: Depth;
    asks: Depth;
  };
  top_ask: number;
  top_bid: number;
};
export interface Transaction {
  price: number;
  quantity: number;
}
export interface SimulationStep {
  timestamp: number;
  depths: {
    bids: Depth;
    asks: Depth;
  };
  top_ask: number;
  top_bid: number;
  transactions: Transaction[];
};
export const convertToSimulationStep = (obj: any): SimulationStep => {
  const timestamp = obj[0] as number;
  const step = obj[1];
  const top_ask = step["top_ask"];
  const top_bid = step["top_bid"];
  const transactions = step["transactions"];
  const bidsRecord = step["depths"][0] as Record<string, number>;
  const asksRecord = step["depths"][1] as Record<string, number>;
  const bids = Object.entries(bidsRecord).reduce((prev, curr) => {
    prev.set(parseFloat(curr[0]), curr[1]);
    return prev;
  }, new Map<number, number>());
  const asks = Object.entries(asksRecord).reduce((prev, curr) => {
    prev.set(parseFloat(curr[0]), curr[1]);
    return prev;
  }, new Map<number, number>());
  return {
    depths: {
      asks,
      bids,
    },
    timestamp,
    top_ask,
    top_bid,
    transactions
  };
};