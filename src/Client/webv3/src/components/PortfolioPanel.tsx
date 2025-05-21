import { useGlobalStore } from "../store";
import ResizableGridTable from "./ResizableGridTable";

export function PortfolioPanel() {
  const tickers = useGlobalStore((s) => s.all_securities);

  const headers = [
    "ticker",
    "type",
    "position",
    "buy vwap",
    "sell vwap",
    "last",
    "realized",
    "nav",
    "volume",
    "my volume",
  ];
  const data = tickers.map((ticker) => {
    return headers.map((header) => {
      if (header === "ticker") {
        return ticker;
      } else if (header === "type") {
        return "stock";
      } else {
        return "0";
      }
    });
  });

  return (
    <div className='flex flex-col gap-2 h-full'>
      <ResizableGridTable headers={headers} data={data} />
    </div>
  );
}
