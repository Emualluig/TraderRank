import { useGlobalStore } from "./store";
import { Panel } from "./components/Panel";
import { useRef } from "react";

let nextId = 1;

function App() {
  const workspaceRef = useRef<HTMLDivElement>(null);
  const panels = useGlobalStore((s) => s.panels);
  const createPanel = useGlobalStore((s) => s.createPanel);

  const addPanel = (type: string) => {
    const id = `${type}-${nextId++}`;
    createPanel(id, 100, 100, 200, 200);
  };

  return (
    <div className='flex flex-col h-screen w-screen relative'>
      {/* Top Bar */}
      <div className='grow-0 h-8 border-b-1 z-10 flex flex-row gap-1 items-center pl-4 pr-4'>
        <button className='bg-amber-400 hover:cursor-pointer' onClick={() => addPanel("book")}>
          <span>Book Panel</span>
        </button>
        <button className='bg-blue-400 hover:cursor-pointer' onClick={() => addPanel("chart")}>
          <span>Chart Panel</span>
        </button>
        <button className='bg-green-400 hover:cursor-pointer' onClick={() => addPanel("clob")}>
          <span>CLOB Panel</span>
        </button>
      </div>
      {/* Workspace */}
      <div
        className='grow bg-gray-400 relative z-0 overflow-clip'
        id='workspace'
        ref={workspaceRef}
      >
        {Object.entries(panels).map(([id]) => (
          <Panel id={id} key={id} title={`Panel ${id}`} workspaceRef={workspaceRef}>
            <div>Content for {id}</div>
          </Panel>
        ))}
      </div>
      {/* Bottom Bar */}
      <div className='grow-0 h-8 border-b-1 z-10 flex flex-row gap-1 items-center pl-4 pr-4'>
        <button className='bg-blue-400 hover:cursor-pointer' id='add-element-button'>
          <span>Add Element</span>
        </button>
        <span>Hold `SHIFT` to disable snapping</span>
        <button className='bg-blue-400 hover:cursor-pointer' id='save-layout-button'>
          <span>Save layout</span>
        </button>
        <div id='simulation-state-label'>
          <span className='text-red-900 font-bold'>CLOSED</span>
        </div>
      </div>
    </div>
  );
}

export default App;
