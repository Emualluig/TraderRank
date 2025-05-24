import { useGlobalStore } from "../store";
import ResizableGridTable from "./ResizableGridTable";
import type { ReactElement } from "react";
import { FlashBackground, FlashDelta } from "./FlashComponents";

type Headers =
  | "Ticker"
  | "Type"
  | "Position"
  | "Cost"
  | "VWAP"
  | "Last"
  | "Bid"
  | "Ask"
  | "Realized"
  | "NLV"
  | "Volume"
  | "My Volume";

const allHeaders = [
  "Ticker",
  "Type",
  "Position",
  "Cost",
  "VWAP",
  "Last",
  "Bid",
  "Ask",
  "Realized",
  "NLV",
  "Volume",
  "My Volume",
] as const satisfies readonly Headers[];

export function PortfolioPanel() {
  const tick = useGlobalStore((s) => s.tick);
  const transactions = useGlobalStore((s) => s.transactions);
  const tickers = useGlobalStore((s) => s.all_securities);
  const portfolio = useGlobalStore((s) => s.portfolio);
  const portfolioDelta = useGlobalStore((s) => s.portfolio_delta);
  const previousBidAsk = useGlobalStore((s) => s.previous_top_bid_ask);
  const orderBook = useGlobalStore((s) => s.order_book_per_security);

  const obj = tickers.map((ticker) => {
    const previousBid = previousBidAsk[ticker]?.bid ?? null;
    const currentBid = orderBook[ticker]?.bid?.at(0) ?? null;
    const previousAsk = previousBidAsk[ticker]?.ask ?? null;
    const currentAsk = orderBook[ticker]?.ask?.at(0) ?? null;

    const currentHoldings = portfolio[ticker] ?? 0;
    const holdingsDelta = portfolioDelta[ticker];

    const lastPreviousTick =
      transactions[ticker].filter((el) => el.tick === tick - 1).at(-1)?.price ?? null;
    const lastThisTick =
      transactions[ticker].filter((el) => el.tick === tick).at(-1)?.price ?? null;

    return {
      Ticker: <span>{ticker}</span>,
      Type: <span>??</span>,
      Position: <FlashDelta value={currentHoldings} delta={holdingsDelta} />,
      Cost: <span>??</span>,
      VWAP: <span>??</span>,
      Last: <FlashBackground current={lastThisTick} previous={lastPreviousTick} />,
      Bid: (
        <FlashBackground
          current={currentBid?.price ?? null}
          previous={previousBid?.price ?? null}
        />
      ),
      Ask: (
        <FlashBackground
          current={currentAsk?.price ?? null}
          previous={previousAsk?.price ?? null}
        />
      ),
      Realized: <span>??</span>,
      NLV: <span>??</span>,
      Volume: <span>??</span>,
      "My Volume": <span>??</span>,
    } satisfies Record<Headers, ReactElement>;
  });

  const headers = Object.values(allHeaders);
  const data = obj.map((el) => Object.values(el));

  return (
    <div className='flex flex-col gap-2 h-full'>
      <ResizableGridTable headers={headers} data={data} />
    </div>
  );
}
