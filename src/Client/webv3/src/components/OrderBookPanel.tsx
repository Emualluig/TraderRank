import { useRef, useState } from "react";
import { useGlobalStore } from "../store";
import IntegerInput, { type IntegerInputRef } from "./IntegerInput";
import "./OrderBookPanel.css";

const DEFAULT_SELECTION = "--";

export function OrderBookPanel() {
  const volumeRef = useRef<IntegerInputRef>(null);
  const offsetRef = useRef<IntegerInputRef>(null);

  const securities = useGlobalStore((state) => state.tradeable_securities);
  const securityInfo = useGlobalStore((state) => state.security_info);
  const orderBooks = useGlobalStore((state) => state.order_book_per_security);
  const userIdMap = useGlobalStore((state) => state.user_id_to_username);
  const [selectedSecurity, setSelectedSecurity] = useState<string>(DEFAULT_SELECTION);

  return (
    <div className='flex flex-col gap-2 h-full'>
      <div className='flex flex-row justify-between'>
        <select
          value={selectedSecurity}
          onChange={(e) => setSelectedSecurity(e.target.value)}
          className='outline-1 w-20 select-none'
        >
          <option value={DEFAULT_SELECTION}>{DEFAULT_SELECTION}</option>
          {securities.map((el) => (
            <option key={el} value={el}>
              {el}
            </option>
          ))}
        </select>
        <div className='flex flex-row gap-1'>
          <label className='flex flex-row gap-0.5'>
            <span>V:</span>
            <IntegerInput
              ref={volumeRef}
              isDisabled={selectedSecurity == DEFAULT_SELECTION}
              min={1}
              max={securityInfo?.[selectedSecurity]?.max_trade_volume ?? 1}
            />
          </label>
          <label className='flex flex-row gap-0.5'>
            <span>O:</span>
            <IntegerInput
              ref={offsetRef}
              isDisabled={selectedSecurity == DEFAULT_SELECTION}
              min={0}
              max={100}
            />
          </label>
        </div>
      </div>
      <div className='flex-grow flex flex-row gap-2'>
        <div className='w-1/2 outline-1 bg-gray-100'>
          <table className='w-full'>
            <thead className='tb-divider'>
              <tr className='bg-gray-700 text-white sticky top-0'>
                <th>
                  <span>Trader</span>
                </th>
                <th>
                  <span>Volume</span>
                </th>
                <th>
                  <span>Price</span>
                </th>
              </tr>
            </thead>
            <tbody className='tb-divider'>
              {orderBooks[selectedSecurity]?.bids?.map((bid) => (
                <tr key={bid.order_id}>
                  <td>{userIdMap[bid.user_id] ?? bid.user_id}</td>
                  <td>{bid.volume}</td>
                  <td>{bid.price}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
        <div className='w-1/2 outline-1 bg-gray-100'>
          <table className='w-full border-collapse'>
            <thead className='tb-divider'>
              <tr className='bg-gray-700 text-white sticky top-0'>
                <th>
                  <span>Price</span>
                </th>
                <th>
                  <span>Volume</span>
                </th>
                <th>
                  <span>Trader</span>
                </th>
              </tr>
            </thead>
            <tbody className='tb-divider'>
              {orderBooks[selectedSecurity]?.asks?.map((ask) => (
                <tr key={ask.order_id}>
                  <td>{ask.volume}</td>
                  <td>{ask.price}</td>
                  <td>{userIdMap[ask.user_id] ?? ask.user_id}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
