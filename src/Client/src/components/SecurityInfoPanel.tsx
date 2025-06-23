import { useGlobalStore } from "../store";
import ResizableGridTable from "./ResizableGridTable";

export function SecurityInfoPanel() {
  const securityInfo = useGlobalStore((s) => s.security_info);
  const portfolio = useGlobalStore((s) => s.portfolio);

  const a = Object.entries(securityInfo).map((el) => {
    const ticker = el[0];
    const info = el[1];
    return [
      ticker,
      `${Math.abs(portfolio[ticker] ?? 0)}/${info.net_limit.toFixed(0)}`,
      `${Math.abs(portfolio[ticker] ?? 0)}/${info.gross_limit.toFixed(0)}`,
      `${info.max_trade_volume.toFixed(0)}`,
    ];
  });

  const headers = ["Ticker", "Net Limit", "Gross Limit", "Max Trade Size"];

  return (
    <div className='flex flex-col gap-2 h-full'>
      <ResizableGridTable headers={headers} data={a} />
    </div>
  );
}
