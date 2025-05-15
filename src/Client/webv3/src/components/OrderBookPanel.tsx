import { useEffect, useRef, useState } from "react";
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

  const [maxTradeVolume, setMaxTradeVolume] = useState(1);
  const [decimalPlaces, setDecimalPlaces] = useState(2);
  useEffect(() => {
    setMaxTradeVolume(securityInfo[selectedSecurity]?.max_trade_volume ?? 1);
    setDecimalPlaces(securityInfo[selectedSecurity]?.decimal_places ?? 2);
  }, [securityInfo, selectedSecurity]);

  return (
    <div className='flex flex-col gap-2 h-full'>
      <div className='flex flex-row justify-between'>
        <select
          value={selectedSecurity}
          onChange={(e) => setSelectedSecurity(e.target.value)}
          className='outline-1 w-22 select-none'
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
              max={maxTradeVolume}
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
      <div className='flex flex-row gap-2 flex-grow flex-shrink overflow-hidden'>
        <div className='w-1/2 border-1 flex flex-col h-full'>
          <div className='grid grid-cols-3 bg-gray-700 text-white sticky top-0 text-sm font-bold'>
            <div className='px-2 py-1'>Trader</div>
            <div className='px-2 py-1 text-right'>Volume</div>
            <div className='px-2 py-1 text-right'>Price</div>
          </div>
          <div className='overflow-y-scroll flex-1'>
            {orderBooks[selectedSecurity]?.bids?.map((order) => (
              <div key={order.order_id} className='grid grid-cols-3 text-sm border-b'>
                <div className='px-2 py-1 truncate'>
                  {userIdMap[order.user_id] ?? order.user_id}
                </div>
                <div className='px-2 py-1 text-right'>{order.volume}</div>
                <div className='px-2 py-1 text-right'>{order.price.toFixed(decimalPlaces)}</div>
              </div>
            ))}
          </div>
        </div>
        <div className='w-1/2 border-1 flex flex-col h-full'>
          <div className='grid grid-cols-3 bg-gray-700 text-white sticky top-0 text-sm font-bold'>
            <div className='px-2 py-1 text-left'>Price</div>
            <div className='px-2 py-1 text-left'>Volume</div>
            <div className='px-2 py-1'>Trader</div>
          </div>

          <div className='overflow-y-scroll flex-1'>
            {orderBooks[selectedSecurity]?.asks?.map((order) => (
              <div key={order.order_id} className='grid grid-cols-3 text-sm border-b'>
                <div className='px-2 py-1 text-left'>{order.price.toFixed(decimalPlaces)}</div>
                <div className='px-2 py-1 text-left'>{order.volume}</div>
                <div className='px-2 py-1 truncate'>
                  {userIdMap[order.user_id] ?? order.user_id}
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
