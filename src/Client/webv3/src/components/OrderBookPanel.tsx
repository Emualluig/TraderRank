import { useEffect, useRef, useState } from "react";
import { useGlobalStore } from "../store";
import IntegerInput, { type IntegerInputRef } from "./IntegerInput";
import "./OrderBookPanel.css";
import type { LimitOrder } from "../types";

const DEFAULT_SELECTION = "--";

interface OrderElement extends HTMLDivElement {
  order: LimitOrder;
  volume_element: HTMLDivElement;
  to_remove: boolean;
}

interface OrderBookTableProps {
  orders: LimitOrder[] | undefined;
  decimalPlaces: number;
  userIdMap: Record<number, string>;
  ticker: string;
}

function OrderBookTable({ orders, decimalPlaces, userIdMap, ticker }: OrderBookTableProps) {
  const isFirstTick = useGlobalStore((s) => s.isFirstTick());
  const innerBookRef = useRef<HTMLDivElement | null>(null);
  const previousOrders = useRef(new Map<number, LimitOrder>());
  const previousOrderIds = useRef(new Set<number>());

  const rebuildBook = () => {
    const root = innerBookRef.current!;
    root.innerHTML = "";
    previousOrderIds.current = new Set();
    previousOrders.current = new Map<number, LimitOrder>();
    for (const order of orders ?? []) {
      const orderElement = document.createElement("div");
      orderElement.classList.add("grid", "grid-cols-3", "text-sm", "border-b");
      (orderElement as OrderElement).order = order;
      (orderElement as OrderElement).to_remove = false;

      const userElement = document.createElement("div");
      userElement.classList.add("px-2", "py-1", "truncate");
      userElement.innerText = `${userIdMap[order.user_id] ?? order.user_id}`;

      const volumeElement = document.createElement("div");
      volumeElement.classList.add("px-2", "py-1", "text-right");
      volumeElement.innerText = `${order.volume}`;
      (orderElement as OrderElement).volume_element = volumeElement;

      const priceElement = document.createElement("div");
      priceElement.classList.add("px-2", "py-1", "text-right");
      priceElement.innerText = order.price.toFixed(decimalPlaces);

      orderElement.append(userElement, volumeElement, priceElement);
      root.append(orderElement);

      previousOrderIds.current.add(order.order_id);
      previousOrders.current.set(order.order_id, order);
    }
  };

  useEffect(() => {
    // Rebuild completely in this case
    if (innerBookRef.current === null) {
      return;
    }
    rebuildBook();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [decimalPlaces, userIdMap, ticker]);

  useEffect(() => {
    // Partially rebuild in this case
    if (innerBookRef.current === null) {
      return;
    }
    const root = innerBookRef.current;
    if (orders === undefined) {
      root.innerHTML = "";
      return;
    }
    if (isFirstTick) {
      // Rebuild completely
      rebuildBook();
      return;
    }

    // Perform delta update

    for (let i = root.children.length - 1; i >= 0; i--) {
      const child = root.children[i] as OrderElement;
      if (child.classList.contains("bg-red-300")) {
        root.removeChild(child);
      } else {
        child.classList.remove("bg-red-300", "bg-green-300", "bg-blue-300");
      }
    }

    const currentOrderIds = orders.reduce((prev, curr) => {
      prev.add(curr.order_id);
      return prev;
    }, new Set<number>());
    const currentOrders = orders.reduce((prev, curr) => {
      prev.set(curr.order_id, curr);
      return prev;
    }, new Map<number, LimitOrder>());
    const addedIds = currentOrderIds.difference(previousOrderIds.current);
    const removedIds = previousOrderIds.current.difference(currentOrderIds);
    const modifiedIds = new Set(
      currentOrderIds
        .intersection(previousOrderIds.current)
        .values()
        .filter((order_id) => {
          const oldOrder = previousOrders.current.get(order_id);
          const newOrder = currentOrders.get(order_id);
          if (
            oldOrder !== undefined &&
            newOrder !== undefined &&
            oldOrder.volume !== newOrder.volume
          ) {
            return true;
          } else {
            return false;
          }
        })
        .toArray()
    );

    const addedOrders = orders.filter((el) => addedIds.has(el.order_id));
    let idx = 0;
    for (const child of Array.from(root.children)) {
      const j = child as OrderElement;
      if (idx < addedOrders.length && j.order.price < addedOrders[idx].price) {
        const b = addedOrders[idx];

        const orderElement = document.createElement("div");
        orderElement.classList.add("grid", "grid-cols-3", "text-sm", "border-b", "bg-green-300");
        (orderElement as OrderElement).order = b;

        const userElement = document.createElement("div");
        userElement.classList.add("px-2", "py-1", "truncate");
        userElement.innerText = `${userIdMap[b.user_id] ?? b.user_id}`;

        const volumeElement = document.createElement("div");
        volumeElement.classList.add("px-2", "py-1", "text-right");
        volumeElement.innerText = `${b.volume}`;
        (orderElement as OrderElement).volume_element = volumeElement;

        const priceElement = document.createElement("div");
        priceElement.classList.add("px-2", "py-1", "text-right");
        priceElement.innerText = b.price.toFixed(decimalPlaces);

        orderElement.append(userElement, volumeElement, priceElement);
        root.insertBefore(orderElement, j);
        idx += 1;
      }
    }
    for (let i = idx; i < addedOrders.length; i++) {
      const b = addedOrders[i];

      const orderElement = document.createElement("div");
      orderElement.classList.add("grid", "grid-cols-3", "text-sm", "border-b", "bg-green-300");
      (orderElement as OrderElement).order = b;

      const userElement = document.createElement("div");
      userElement.classList.add("px-2", "py-1", "truncate");
      userElement.innerText = `${userIdMap[b.user_id] ?? b.user_id}`;

      const volumeElement = document.createElement("div");
      volumeElement.classList.add("px-2", "py-1", "text-right");
      volumeElement.innerText = `${b.volume}`;
      (orderElement as OrderElement).volume_element = volumeElement;

      const priceElement = document.createElement("div");
      priceElement.classList.add("px-2", "py-1", "text-right");
      priceElement.innerText = b.price.toFixed(decimalPlaces);

      orderElement.append(userElement, volumeElement, priceElement);
      root.append(orderElement);
    }

    for (const child of Array.from(root.children)) {
      const j = child as OrderElement;
      const order_id = j.order.order_id;
      if (removedIds.has(order_id)) {
        j.classList.add("bg-red-300");
      } else if (modifiedIds.has(order_id)) {
        j.classList.add("bg-blue-300");
        j.order = currentOrders.get(order_id)!;
        j.volume_element.innerText = `${j.order.volume}`;
      }
    }

    previousOrderIds.current = currentOrderIds;
    previousOrders.current = currentOrders;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [orders]);

  return <div className='overflow-y-scroll flex-1' ref={innerBookRef}></div>;
}

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
          <OrderBookTable
            orders={orderBooks[selectedSecurity]?.bids}
            decimalPlaces={decimalPlaces}
            userIdMap={userIdMap}
            ticker={selectedSecurity}
          />
        </div>
        <div className='w-1/2 border-1 flex flex-col h-full'>
          <div className='grid grid-cols-3 bg-gray-700 text-white sticky top-0 text-sm font-bold'>
            <div className='px-2 py-1 text-left'>Price</div>
            <div className='px-2 py-1 text-left'>Volume</div>
            <div className='px-2 py-1'>Trader</div>
          </div>

          <div className='overflow-y-scroll flex-1'>
            {orderBooks[selectedSecurity]?.bids?.map(
              (order /* TODO: Change this back to ask  */) => (
                <div key={order.order_id} className='grid grid-cols-3 text-sm border-b'>
                  <div className='px-2 py-1 text-left'>{order.price.toFixed(decimalPlaces)}</div>
                  <div className='px-2 py-1 text-left'>{order.volume}</div>
                  <div className='px-2 py-1 truncate'>
                    {userIdMap[order.user_id] ?? order.user_id}
                  </div>
                </div>
              )
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
