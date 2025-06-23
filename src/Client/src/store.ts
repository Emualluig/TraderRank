import { create } from "zustand";
import { immer } from "zustand/middleware/immer";
import type { PanelType } from "./components/Panel";
import type {
  BidAskStruct,
  LimitOrder,
  LimitOrderExt,
  MessageProcessor,
  News,
  OrderBook,
  OrderBookHighlight,
  SecurityInfo,
  SimulationState,
  Ticker,
  Transaction,
  UserID,
  Username,
} from "./types";

interface Layout {
  workspaceX: number;
  workspaceY: number;
  width: number;
  height: number;
  zIndex: number;
  minSize: Interact.Size;
  maxSize: Interact.Size;
  type: PanelType;
}

interface GlobalState extends MessageProcessor {
  // Market state data
  is_initialized: boolean;
  tick: number;
  max_tick: number;
  user_id: UserID;
  simulation_state: SimulationState;
  all_securities: Ticker[];
  tradeable_securities: Ticker[];
  security_info: Record<Ticker, SecurityInfo>;
  order_book_per_security: Record<Ticker, OrderBook>;
  order_book_per_security_color: Record<Ticker, OrderBookHighlight>;
  previous_top_bid_ask: Record<Ticker, BidAskStruct<LimitOrder | null>>;
  user_id_to_username: Record<UserID, Username>;
  portfolio: Record<Ticker, number>;
  portfolio_delta: Record<Ticker, number>;
  transactions: Record<Ticker, Transaction[]>;
  new_transactions: Record<Ticker, Transaction[]>;
  news: News[];
  news_read: boolean[];
  chat: Array<{ user_id: UserID; text: string }>;
  chat_read: boolean[];

  // Panel data
  topZ: number;
  bringToFront: (id: string) => void;
  panels: Record<string, Layout>;
  createPanel: (
    id: string,
    x: number,
    y: number,
    w: number,
    h: number,
    minSize: Interact.Size,
    maxSize: Interact.Size,
    type: PanelType
  ) => void;
  setPanelPosition: (id: string, x: number, y: number) => void;
  setPanelSize: (id: string, w: number, h: number) => void;
  removePanel: (id: string) => void;
}

export const useGlobalStore = create<GlobalState>()(
  immer((set) => ({
    is_initialized: false,
    tick: 0,
    max_tick: 0,
    user_id: 0,
    simulation_state: "paused",
    all_securities: [],
    tradeable_securities: [],
    security_info: {},
    order_book_per_security: {},
    order_book_per_security_color: {},
    previous_top_bid_ask: {},
    user_id_to_username: {},
    portfolio: {},
    portfolio_delta: {},
    transactions: {},
    new_transactions: {},
    news: [],
    news_read: [],
    chat: [],
    chat_read: [],

    processMessageLoginRequest: () => {
      throw new Error("??");
    },
    processMessageLoginResponse: (msg) =>
      set((state) => {
        state.is_initialized = true;
        state.user_id = msg.user_id;
      }),
    processMessageSimulationLoad: (msg) =>
      set((state) => {
        state.tick = msg.tick;
        state.max_tick = msg.max_tick;
        state.simulation_state = msg.simulation_state;
        state.all_securities = msg.all_securities;
        state.tradeable_securities = msg.tradeable_securities;
        state.security_info = msg.security_info;
        state.order_book_per_security = msg.order_book_per_security;
        state.order_book_per_security_color = structuredClone(msg.order_book_per_security);
        state.previous_top_bid_ask = Object.entries(msg.order_book_per_security).reduce(
          (prev, curr) => {
            const ticker = curr[0];
            const book = curr[1];
            prev[ticker] = {
              bid: book.bid.at(0) ?? null,
              ask: book.ask.at(0) ?? null,
            };
            return prev;
          },
          {} as Record<Ticker, BidAskStruct<LimitOrder | null>>
        );
        state.user_id_to_username = msg.user_id_to_username;
        state.portfolio = msg.portfolio;
        state.portfolio_delta = Object.keys(msg.portfolio).reduce(
          (prev, curr) => {
            prev[curr] = 0;
            return prev;
          },
          {} as Record<string, number>
        );
        state.transactions = msg.transactions;
        state.new_transactions = structuredClone(msg.transactions);
        state.news = msg.news;
        state.news_read = msg.news.map(() => false);
        state.chat = [];
        state.chat_read = [];
      }),
    processMessageSimulationUpdate: (msg) =>
      set((state) => {
        state.tick = msg.tick;
        state.simulation_state = msg.simulation_state;
      }),
    processMessageMarketUpdate: (msg) =>
      set((state) => {
        state.tick = msg.tick;

        const submitted_orders = msg.submitted_orders;
        const cancelled_orders = msg.cancelled_orders;
        const transacted_orders = msg.transacted_orders;

        for (const ticker of state.all_securities) {
          state.previous_top_bid_ask[ticker].bid =
            state.order_book_per_security[ticker].bid.at(0) ?? null;
          state.previous_top_bid_ask[ticker].ask =
            state.order_book_per_security[ticker].ask.at(0) ?? null;
        }

        for (const ticker of state.all_securities) {
          // const previous_book = state.order_book_per_security[ticker];
          const previous_book_color = state.order_book_per_security_color[ticker];
          const local_submitted_orders = submitted_orders[ticker];
          const local_cancelled_orders = new Set(cancelled_orders[ticker]);
          const local_transacted_orders = new Map(transacted_orders[ticker]);

          const current_book_bid_color = previous_book_color.bid
            .filter((el) => el.color !== "red" && el.volume !== 0)
            .map((el) => {
              return {
                ...el,
                change: undefined,
                color: undefined,
                isNew: undefined,
              } as LimitOrderExt;
            })
            .concat(
              local_submitted_orders.bid.map((el) => {
                return {
                  ...el,
                  color: "green",
                  isNew: true,
                } satisfies LimitOrderExt;
              })
            )
            .map((el) => {
              const transacted_volume = local_transacted_orders.get(el.order_id);
              if (transacted_volume !== undefined) {
                el.change = transacted_volume;
                el.volume -= transacted_volume;
                el.color = "red";
              }
              const is_cancelled = local_cancelled_orders.has(el.order_id);
              if (is_cancelled) {
                el.color = "red";
              }
              return el;
            })
            .sort((a, b) => {
              const s = b.price - a.price;
              if (s === 0) {
                return a.order_id - b.order_id;
              }
              return s;
            });

          const current_book_ask_color = previous_book_color.ask
            .filter((el) => el.color !== "red" && el.volume !== 0)
            .map((el) => {
              return {
                ...el,
                color: undefined,
                change: undefined,
                isNew: undefined,
              } as LimitOrderExt;
            })
            .concat(
              local_submitted_orders.ask.map((el) => {
                return {
                  ...el,
                  color: "green",
                  isNew: true,
                } satisfies LimitOrderExt;
              })
            )
            .map((el) => {
              const transacted_volume = local_transacted_orders.get(el.order_id);
              if (transacted_volume !== undefined) {
                el.change = transacted_volume;
                el.volume -= transacted_volume;
                el.color = "red";
              }
              const is_cancelled = local_cancelled_orders.has(el.order_id);
              if (is_cancelled) {
                el.color = "red";
              }
              return el;
            })
            .sort((a, b) => {
              const s = a.price - b.price;
              if (s === 0) {
                return a.order_id - b.order_id;
              }
              return s;
            });

          state.order_book_per_security[ticker].bid = msg.order_book_per_security[ticker].bid;
          state.order_book_per_security[ticker].ask = msg.order_book_per_security[ticker].ask;
          state.order_book_per_security_color[ticker].bid = current_book_bid_color;
          state.order_book_per_security_color[ticker].ask = current_book_ask_color;
        }

        for (const [k, v] of Object.entries(msg.portfolio)) {
          state.portfolio_delta[k] = v - state.portfolio[k];
        }
        state.portfolio = msg.portfolio;

        for (const ticker of state.all_securities) {
          for (const transaction of msg.new_transactions[ticker]) {
            state.transactions[ticker].push(transaction);
          }
        }

        for (const news of msg.new_news) {
          state.news.push(news);
          state.news_read.push(false);
        }
      }),
    processMessageNewUserConnected: (msg) =>
      set((state) => {
        state.user_id_to_username[msg.user_id] = msg.username;
      }),
    processMessageChatMessageReceived: (msg) =>
      set((state) => {
        state.chat.push(msg);
        state.chat_read.push(false);
      }),

    // Panel items
    topZ: 0,
    bringToFront: (id) =>
      set((state) => {
        if (state.panels[id]) {
          state.topZ += 1;
          state.panels[id].zIndex = state.topZ;
        }
      }),
    panels: {},
    createPanel: (id, x, y, w, h, minSize, maxSize, type) =>
      set((state) => {
        if (state.panels[id] === undefined) {
          state.panels[id] = {
            workspaceX: x,
            workspaceY: y,
            width: w,
            height: h,
            zIndex: state.topZ + 1,
            minSize: minSize,
            maxSize: maxSize,
            type: type,
          };
          state.topZ += 1;
        }
      }),
    setPanelPosition: (id, x, y) =>
      set((state) => {
        if (state.panels[id]) {
          state.panels[id].workspaceX = x;
          state.panels[id].workspaceY = y;
        }
      }),
    setPanelSize: (id, w, h) =>
      set((state) => {
        if (state.panels[id]) {
          state.panels[id].width = w;
          state.panels[id].height = h;
        }
      }),
    removePanel: (id) =>
      set((state) => {
        delete state.panels[id];
      }),
  }))
);
