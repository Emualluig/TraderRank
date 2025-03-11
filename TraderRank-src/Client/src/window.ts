import interact from 'interactjs';
import * as d3 from "d3";

export interface WindowElement extends HTMLDivElement {
    workspaceX: number;
    workspaceY: number;
    dragX: number;
    dragY: number;
    initialTopY: number | null;
    initialBottomY: number | null;
    minDimensions: Interact.Size | undefined;
    maxDimensions: Interact.Size | undefined;
    frameRef: WindowFrame;
  }

  const doIntervalsOverlap = (start1: number, end1: number, start2: number, end2: number): boolean => {
    // https://nedbatchelder.com/blog/201310/range_overlap_in_two_compares.html
    return (end1 >= start2) && (end2 >= start1)
  }

  const DRAG_SNAP_THRESHOLD = 10;

  const makeDraggable = (windowElement: WindowElement, WORKSPACE: HTMLElement, WINDOW_MAP: Map<string, WindowElement>) => {
    interact(windowElement).draggable({
      allowFrom: ".window-top",
      modifiers: [
        interact.modifiers.restrictRect({
          restriction: WORKSPACE,
        }),
      ],
      listeners: {
        start: (event: Interact.DragEvent) => {
          const target = event.target as WindowElement;
          target.dragX = target.workspaceX;
          target.dragY = target.workspaceY;
        },
        move: (event: Interact.DragEvent) => {
          const workspaceRect = WORKSPACE.getBoundingClientRect();
          const workspaceX = workspaceRect.x;
          const workspaceY = workspaceRect.y;
  
          const target = event.target as WindowElement;
          const selfRect = target.getBoundingClientRect();
          const selfTopY = selfRect.top;
          const selfBottomY = selfRect.bottom;
          const selfLeftX = selfRect.left;
          const selfRightX = selfRect.right;
          const selfWidth = selfRect.width;
          const selfHeight = selfRect.height;
  
          const dragX = target.dragX += event.dx;
          const dragY = target.dragY += event.dy;
  
          let finalX = dragX;
          let finalY = dragY;
          if (!event.shiftKey) {
            for (const otherWindow of WINDOW_MAP.values()) {
              if (otherWindow === target) {
                // Prevent self-collisions
                continue;
              }
              const otherRect = otherWindow.getBoundingClientRect();
              const otherTopY = otherRect.top;
              const otherBottomY = otherRect.bottom;
              const otherLeftX = otherRect.left;
              const otherRightX = otherRect.right;
    
              if (doIntervalsOverlap(otherTopY, otherBottomY, selfTopY, selfBottomY)) {
                if (Math.abs(dragX - otherRightX) < DRAG_SNAP_THRESHOLD) {
                  // Snap the self left to the other right
                  finalX = otherRightX - workspaceX;
                }
                if (Math.abs(dragX - (otherLeftX - selfWidth)) < DRAG_SNAP_THRESHOLD) {
                  // snap the self right to the other left
                  finalX = otherLeftX - selfWidth - workspaceX;
                }
              }
              if (doIntervalsOverlap(otherLeftX, otherRightX, selfLeftX, selfRightX)) {
                if (Math.abs(dragY - (otherBottomY - workspaceY)) < DRAG_SNAP_THRESHOLD) {
                  // Snap the self top to the other bottom
                  finalY = otherBottomY - workspaceY;
                }
                if (Math.abs(dragY - (otherTopY - selfHeight - workspaceY)) < DRAG_SNAP_THRESHOLD) {
                  // Snap the self bottom to other top
                  finalY = otherTopY - selfHeight - workspaceY;
                }
              }
            }
          }
  
          target.style.transform = `translate(${finalX}px, ${finalY}px)`;
          target.workspaceX = finalX;
          target.workspaceY = finalY;
        }
      }
    });
  };

// Basic, no snapping
const makeResizable = (windowElement: WindowElement, WORKSPACE: HTMLElement, WINDOW_MAP: Map<string, WindowElement>) => {
    interact(windowElement).resizable({
      edges: { left: true, right: true,  bottom: true }, // Enable all edges
      modifiers: [
        interact.modifiers.restrictEdges({
          outer: WORKSPACE, // Prevent resizing beyond workspace
        }),
        interact.modifiers.restrictSize({
          min: windowElement.minDimensions,
          max: windowElement.maxDimensions,
        }),
      ],
      listeners: {
        move: (event: Interact.ResizeEvent) => {
          const target = event.target as WindowElement;
  
          let x = target.workspaceX;
          let y = target.workspaceY;
  
          let { width, height } = event.rect; // New size from interact.js
  
          const workspaceRect = WORKSPACE.getBoundingClientRect();
  
          // Ensure the window doesn't overflow workspace
          const maxWidth = workspaceRect.width - x;
          const maxHeight = workspaceRect.height - y;
  
          width = Math.min(width, maxWidth);
          height = Math.min(height, maxHeight);
  
          // Adjust position when resizing from the left
          if (event.edges!.left) {
            const deltaX = event.deltaRect!.left; // How much the left side moved
            x += deltaX;
            width -= deltaX;
          }
  
          // Adjust position when resizing from the top
          if (event.edges!.top) {
            const deltaY = event.deltaRect!.top; // How much the top side moved
            y += deltaY;
            height -= deltaY;
          }
  
          target.style.width = `${width}px`;
          target.style.height = `${height}px`;
  
          target.style.transform = `translate(${x}px, ${y}px)`;
  
          target.workspaceX = x;
          target.workspaceY = y;
        }
      }
    });
  };

  const makeResizableSnap = (windowElement: WindowElement, WORKSPACE: HTMLElement, WINDOW_MAP: Map<string, WindowElement>) => {
    interact(windowElement).resizable({
      edges: { left: true, right: true,  bottom: true, top: true }, // Enable all edges
      modifiers: [
        interact.modifiers.restrictEdges({
          outer: WORKSPACE, // Prevent resizing beyond workspace
        }),
        interact.modifiers.restrictSize({
          min: windowElement.minDimensions,
          max: windowElement.maxDimensions,
        }),
      ],
      listeners: {
        start: (event: Interact.ResizeEvent) => {
          const target = event.target as WindowElement;
          const selfRect = target.getBoundingClientRect();
          target.initialTopY = selfRect.top;
          target.initialBottomY = selfRect.bottom;
        },
        move: (event: Interact.ResizeEvent) => {
          const target = event.target as WindowElement;
  
          const rectHeight = event.rect.height;
          const rectWidth = event.rect.width;
          let snappedOtherTopY: number|null = null;
          let snappedOtherBottomY: number|null = null;
  
          const workspaceRect = WORKSPACE.getBoundingClientRect();
          const workspaceX = workspaceRect.x;
          const workspaceY = workspaceRect.y;
  
          const selfRect = target.getBoundingClientRect();
          const selfTopY = selfRect.top;
          const selfBottomY = selfRect.bottom;
          const selfLeftX = selfRect.left;
          const selfRightX = selfRect.right;
          if (!event.shiftKey) {
            for (const otherWindow of WINDOW_MAP.values()) {
              if (otherWindow === target) {
                continue;
              }
              const otherRect = otherWindow.getBoundingClientRect();
              const otherTopY = otherRect.top;
              const otherBottomY = otherRect.bottom;
              const otherLeftX = otherRect.left;
              const otherRightX = otherRect.right;
    
              if (event.edges?.top) {
                if (doIntervalsOverlap(otherTopY, otherBottomY, selfTopY, selfBottomY) &&
                  (Math.abs(otherRightX - selfLeftX) === 0 || Math.abs(otherLeftX - selfRightX) === 0) &&
                  Math.abs(-rectHeight + target.initialBottomY! - otherTopY) < DRAG_SNAP_THRESHOLD) {
                  console.log("SNAPPED to top of other", target.id, otherWindow.id, rectHeight);
                  snappedOtherTopY = otherTopY - workspaceY;
                }
              }
              if (event.edges?.bottom) {
                if (doIntervalsOverlap(otherTopY, otherBottomY, selfTopY, selfBottomY) &&
                  (Math.abs(otherRightX - selfLeftX) === 0 || Math.abs(otherLeftX - selfRightX) === 0) &&
                  Math.abs(rectHeight + target.initialTopY! - otherBottomY) < DRAG_SNAP_THRESHOLD) {
                  console.log("SNAPPED to bottom of other", target.id, otherWindow.id);
                  snappedOtherBottomY = otherBottomY;
                }
              }
            }
          }
  
          let y = target.workspaceY;
          let height = rectHeight;
          if (event.edges?.top) {
            if (snappedOtherTopY) {
              y = snappedOtherTopY;
              height = y - target.initialBottomY!;
            } else {
              y += event.deltaRect!.top;
              height -= event.deltaRect!.top;
            }
          }
  
          const x = target.workspaceX;
          const width = rectWidth;
  
          // width = Math.min(width, maxWidth);
          // height = Math.min(height, maxHeight);
  
          // // Adjust position when resizing from the top
          // if (event.edges!.top) {
          //   if (snappedOtherTopY != null) {
          //     console.log("it is true")
          //     y = snappedOtherTopY - workspaceY;
          //     height = selfBottomY - snappedOtherTopY;
          //   } else {
          //     const deltaY = event.deltaRect!.top; // How much the top side moved
          //     y += deltaY;
          //     height -= deltaY;
          //   }
          // }
  
          target.style.width = `${width}px`;
          target.style.height = `${height}px`;
          target.style.transform = `translate(${x}px, ${y}px)`;
          target.workspaceX = x;
          target.workspaceY = y;
        }
      }
    });
  };

  const addWindowEventHandlers = (windowElement: WindowElement, WORKSPACE: HTMLElement, WINDOW_MAP: Map<string, WindowElement>, WINDOW_FRAME_MAP: Map<string, WindowFrame>) => {
    // Bring window to front when clicked
    windowElement.addEventListener("mousedown", () => {
      windowElement.style.zIndex = (highestZIndex++).toString();
    });
  
    // Close button functionality
    const closeButton = windowElement.querySelector(".close-btn") as HTMLButtonElement;
    closeButton.addEventListener("click", () => {
      WINDOW_MAP.delete(windowElement.id);
      WINDOW_FRAME_MAP.delete(windowElement.id);
      windowElement.remove();
    });
  };

  let highestZIndex = 1;

export abstract class WindowFrame {
    private readonly DEFAULT_DIMENSIONS: Interact.Size = { width: 250, height: 150 };
    private readonly _id: string;
    protected readonly element: WindowElement;
    protected readonly content: HTMLDivElement;

    protected constructor(
        WORKSPACE: HTMLElement, 
        WINDOW_MAP: Map<string, WindowElement>, 
        WINDOW_FRAME_MAP: Map<string, WindowFrame>,
        dimensions: Interact.Size|null,
        maxDimensions: Interact.Size|null,
        minDimensions: Interact.Size|null,
        title: string,
    ) {
        this._id = `window_${crypto.randomUUID().slice(0, 8)}`;
        const windowElement = document.createElement("div");
        windowElement.id = this._id;
        windowElement.className = "window absolute bg-white border border-gray-700 shadow-lg";
        const dim = dimensions ?? this.DEFAULT_DIMENSIONS;
        windowElement.style.width = `${dim.width}px`;
        windowElement.style.height = `${dim.height}px`;
        windowElement.style.zIndex = (highestZIndex++).toString();
        
        windowElement.innerHTML = `
            <div class="window-top bg-blue-500 text-white p-2 cursor-grab flex justify-between">
                <span>${title}</span>
                <button class="close-btn text-white font-bold px-2 hover:bg-red-500 hover:cursor-pointer">X</button>
            </div>
            <div class="window-content p-4 relative"></div>
        `;

        const contentDiv = windowElement.querySelector("div.window-content");
        if (contentDiv === null) {
            throw new Error("Could not find content");
        }
        this.content = contentDiv as HTMLDivElement;

        const WE = windowElement as WindowElement;
        WE.workspaceX = 50;
        WE.workspaceY = 50;
        WE.dragX = WE.workspaceX;
        WE.dragY = WE.workspaceY;
        WE.initialTopY = null;
        WE.initialBottomY = null;
        WE.minDimensions = minDimensions ?? undefined;
        WE.maxDimensions = maxDimensions ?? undefined;
        WE.frameRef = this;
        this.element = WE;

        WE.style.transform = `translate(${WE.workspaceX}px, ${WE.workspaceY}px)`;

        WINDOW_MAP.set(this._id, WE);
        WINDOW_FRAME_MAP.set(this._id, this);
        WORKSPACE.appendChild(WE);

        makeDraggable(WE, WORKSPACE, WINDOW_MAP);
        makeResizable(WE, WORKSPACE, WINDOW_MAP);
        addWindowEventHandlers(WE, WORKSPACE, WINDOW_MAP, WINDOW_FRAME_MAP);
    }

    public abstract Notify(obj: any): void;

    public get id(): string {
        return this._id;
    };
}

export class WindowDEV extends WindowFrame {
    public constructor(
        WORKSPACE: HTMLElement, 
        WINDOW_MAP: Map<string, WindowElement>,
        WINDOW_FRAME_MAP: Map<string, WindowFrame>
    ) {
        super(
            WORKSPACE, 
            WINDOW_MAP, 
            WINDOW_FRAME_MAP, null, null, { height: 100, width: 150 },
            "Drag Me"
        );
        this.content.innerText = "New Window";
    }   
    public Notify(obj: any): void {}
}

interface Transaction {
    price: number;
    quantity: number;
}

type Depth = Record<string, number>;
interface SimulationStep {
    depths: [Depth, Depth];
    top_ask: number;
    top_bid: number;
    transactions: Transaction[];
}

export class WindowClob extends WindowFrame {
    private readonly orderbookId = `orderbook-${this.id}`;
    private readonly graphHeight = 400;
    private readonly graphWidth = 400;
    private readonly graphMargins = { top: 20, right: 20, bottom: 20, left: 20 };
    public constructor(
        WORKSPACE: HTMLElement, 
        WINDOW_MAP: Map<string, WindowElement>,
        WINDOW_FRAME_MAP: Map<string, WindowFrame>
    ) {
        super(
            WORKSPACE, 
            WINDOW_MAP, 
            WINDOW_FRAME_MAP, { width: 500, height: 500 }, null, { width: 500, height: 500 },
            "CLOB Depth View"
        );
        this.content.innerHTML = `<div id="${this.orderbookId}"></div>`;
    }   
    public Notify(obj: any): void {
        const timestamp = obj[0];
        const simulationStep = obj[1] as SimulationStep;

        const toMap = (depth: Depth): Map<number, number> => {
            const retval = new Map<number, number>();
            for (const [k, v] of Object.entries(depth)) {
                retval.set(parseFloat(k), v);
            }
            return retval;
        }

        const bidDepth = toMap(simulationStep.depths[0]);
        const askDepth = toMap(simulationStep.depths[1]);

        // Convert Depth Map to sorted arrays
        const bidData = Array.from(bidDepth.entries()).sort((a, b) => a[0] - b[0]);
        const askData = Array.from(askDepth.entries()).sort((a, b) => a[0] - b[0]);

        // Scales
        const xScale = d3.scaleLinear()
            .domain([
                Math.min(...bidData.map(d => d[0]), ...askData.map(d => d[0])),
                Math.max(...bidData.map(d => d[0]), ...askData.map(d => d[0]))
            ])
            .range([0, this.graphWidth]);

        const yScale = d3.scaleLinear()
            .domain([0, d3.max([...bidData, ...askData], d => d[1]) || 1])
            .range([this.graphHeight, 0]);

        // Area Generators
        const bidArea = d3.area<[number, number]>()
            .x(d => xScale(d[0]))
            .y0(this.graphHeight)
            .y1(d => yScale(d[1]));

        const askArea = d3.area<[number, number]>()
            .x(d => xScale(d[0]))
            .y0(this.graphHeight)
            .y1(d => yScale(d[1]));

        // Draw Bid Depth
        this.svg.selectAll(".bid-area").remove();
        this.svg.append("path")
            .datum(bidData)
            .attr("class", "bid-area")
            .attr("fill", "green")
            .attr("opacity", 0.5)
            .attr("d", bidArea);

        // Draw Ask Depth
        this.svg.selectAll(".ask-area").remove();
        this.svg.append("path")
            .datum(askData)
            .attr("class", "ask-area")
            .attr("fill", "red")
            .attr("opacity", 0.5)
            .attr("d", askArea);

        // Update Axes
        this.svg.selectAll(".axis").remove();
        this.svg.append("g")
            .attr("class", "axis")
            .attr("transform", `translate(0,${this.graphHeight})`)
            .call(d3.axisBottom(xScale));

        this.svg.append("g")
            .attr("class", "axis")
            .call(d3.axisLeft(yScale));
    }
}
