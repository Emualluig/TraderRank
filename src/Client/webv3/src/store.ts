import { create } from "zustand";
import { immer } from "zustand/middleware/immer";
import type { PanelType } from "./components/Panel";
import type {
  MessageProcessor,
  News,
  OrderBook,
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
  transactions: Record<Ticker, Transaction[]>;
  user_id_to_username: Record<UserID, Username>;
  portfolio: Record<Ticker, number>;
  news: News[];
  news_read: boolean[];
  chat: Array<{ user_id: UserID; text: string }>;
  chat_read: boolean[];

  // Utilities
  isFirstTick: () => boolean;
  isLastTick: () => boolean;

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
  immer((set, get) => ({
    is_initialized: false,
    tick: 0,
    max_tick: 0,
    user_id: 0,
    simulation_state: "paused",
    all_securities: [],
    tradeable_securities: [],
    security_info: {},
    order_book_per_security: {},
    transactions: {},
    user_id_to_username: {},
    portfolio: {},
    news: [],
    news_read: [],
    chat: [],
    chat_read: [],

    isFirstTick: () => get().tick <= 0,
    isLastTick: () => get().tick >= get().max_tick,

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
        state.transactions = msg.transactions;
        state.user_id_to_username = msg.user_id_to_username;
        state.portfolio = msg.portfolio;
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
        state.order_book_per_security = msg.order_book_per_security;
        for (const [ticker, transactions] of Object.entries(msg.new_transactions)) {
          const current_transactions = state.transactions[ticker];
          for (const transaction of transactions) {
            current_transactions.push(transaction);
          }
        }
        state.portfolio = msg.portfolio;
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
