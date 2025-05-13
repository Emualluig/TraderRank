import { useGlobalStore } from "./store";
import { Panel } from "./components/Panel";
import { useRef } from "react";
import { OrderBookPanel } from "./components/OrderBookPanel";
import { TraderInfoPanel } from "./components/TraderInfoPanel";
import clsx from "clsx";

let nextId = 1;

type PanelType = "OrderBook" | "Chart" | "Depth" | "TradeBlotter" | "TraderInfo" | "Portfolio";

function renderPanelContent(type: PanelType) {
  if (type === "OrderBook") {
    return <OrderBookPanel />;
  } else if (type === "TraderInfo") {
    return <TraderInfoPanel />;
  } else {
    return <div>Unknown panel: {type}</div>;
  }
}

function App() {
  const workspaceRef = useRef<HTMLDivElement>(null);
  const panels = useGlobalStore((s) => s.panels);
  const createPanel = useGlobalStore((s) => s.createPanel);
  const tick = useGlobalStore((s) => s.tick);
  const maxTick = useGlobalStore((s) => s.max_tick);
  const state = useGlobalStore((s) => s.simulation_state);
  const unreadNewCount: number = useGlobalStore((s) =>
    s.news_read.reduce((acc, v) => acc + (v ? 0 : 1), 0)
  );
  const unreadChatCount: number = useGlobalStore((s) =>
    s.chat_read.reduce((acc, v) => acc + (v ? 0 : 1), 0)
  );

  const addPanel = (type: PanelType) => {
    const id = `${type}-${nextId++}`;
    if (type === "OrderBook") {
      createPanel(
        id,
        100,
        100,
        325,
        250,
        { height: 250, width: 325 },
        { height: Infinity, width: Infinity },
        type
      );
    } else if (type === "TraderInfo") {
      createPanel(
        id,
        100,
        100,
        275,
        200,
        { height: 200, width: 275 },
        { height: Infinity, width: Infinity },
        type
      );
    } else {
      throw new Error("Unknown panel type.");
    }
  };

  return (
    <div className='flex flex-col h-screen w-screen relative'>
      {/* Top Bar */}
      <div className='grow-0 h-8 border-b-1 z-10 flex flex-row gap-1 items-center pl-4 pr-4'>
        <button className='bg-amber-400 hover:cursor-pointer' onClick={() => addPanel("OrderBook")}>
          <span>Book Panel</span>
        </button>
        <button className='bg-blue-400 hover:cursor-pointer' onClick={() => addPanel("TraderInfo")}>
          <span>Trader Info Panel</span>
        </button>
      </div>
      {/* Workspace */}
      <div className='grow bg-gray-400 relative z-0 overflow-clip' ref={workspaceRef}>
        {Object.entries(panels).map(([id, layout]) => (
          <Panel id={id} key={id} title={`Panel ${id}`} workspaceRef={workspaceRef}>
            {renderPanelContent(layout.type)}
          </Panel>
        ))}
      </div>
      {/* Bottom Bar */}
      <div className='grow-0 h-8 border-t-1 z-10 flex flex-row gap-2.5 items-center pl-4 pr-4'>
        <div>
          <span>
            Progress: {tick}/{maxTick}
          </span>
        </div>
        <div>
          <span
            className={clsx(
              "font-bold",
              state === "paused" && "text-red-800",
              state === "running" && "text-green-600"
            )}
          >
            {state.toUpperCase()}
          </span>
        </div>
        <div>
          <span className='relative inline-flex outline-1 px-1'>
            <button className='cursor-pointer'>
              <span>News</span>
              {unreadNewCount !== 0 && <span className='text-red-800'> ({unreadNewCount})</span>}
            </button>
            {unreadNewCount !== 0 && (
              <span className='absolute top-0 right-0 -mr-0.5 -mt-0.5 flex size-2'>
                <span className='absolute inline-flex h-full w-full animate-ping rounded-full bg-red-700 opacity-75'></span>
                <span className='relative inline-flex size-2 rounded-full bg-red-700'></span>
              </span>
            )}
          </span>
        </div>
        <div>
          <span className='relative inline-flex outline-1 px-1'>
            <button className='cursor-pointer'>
              <span>Chat</span>
              {unreadChatCount !== 0 && <span className='text-red-800'> ({unreadChatCount})</span>}
            </button>
            {unreadChatCount !== 0 && (
              <span className='absolute top-0 right-0 -mr-0.5 -mt-0.5 flex size-2'>
                <span className='absolute inline-flex h-full w-full animate-ping rounded-full bg-red-700 opacity-75'></span>
                <span className='relative inline-flex size-2 rounded-full bg-red-700'></span>
              </span>
            )}
          </span>
        </div>
      </div>
    </div>
  );
}

export default App;
