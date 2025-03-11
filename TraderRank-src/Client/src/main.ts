import interact from 'interactjs';
import './style.css';
import './tw.css';

const LABEL_ID = `#simulation-state-label`;
const WS = new WebSocket("http://localhost:8765");
WS.onopen = () => {
  const label = document.querySelector(LABEL_ID)
  if (label !== null) {
    label.innerHTML = `<span class="text-green-600 font-bold">ACTIVE</span>`;
  }
  console.log("Connected to server");
}
WS.onerror = () => {
  const label = document.querySelector(LABEL_ID)
  if (label !== null) {
    label.innerHTML = `<span class="text-red-900 font-bold">ERROR</span>`;
  }
  console.log("On error!");
}
WS.close = () => {
  const label = document.querySelector(LABEL_ID)
  if (label !== null) {
    label.innerHTML = `<span class="text-amber-600 font-bold">CLOSED</span>`;
  }
  console.log("Disconnected from server!");
}
WS.onmessage = (event) => {
  console.log("Received:", event.data);
}


const WORKSPACE = document.getElementById("workspace")
const ADD_ELEMENT_BUTTON = document.getElementById("add-element-button")
if (!WORKSPACE || !ADD_ELEMENT_BUTTON) {
  throw new Error("Invariant: Couldn't find workspace or add button.");
}
ADD_ELEMENT_BUTTON.addEventListener("click", () => {
  createWindow();
});

interface WindowElement extends HTMLDivElement {
  workspaceX: number;
  workspaceY: number;
  dragX: number;
  dragY: number;
  initialTopY: number | null;
  initialBottomY: number | null;
  minDimensions: Interact.Size | undefined;
  maxDimensions: Interact.Size | undefined;
}

const WINDOW_MAP = new Map<string, WindowElement>();
let highestZIndex = 1;

const createWindow = () => {
  const WINDOW_ID = `window_${crypto.randomUUID().slice(0, 8)}`;
  const windowElement = document.createElement("div");
  windowElement.id = WINDOW_ID;
  windowElement.className = "window absolute bg-white border border-gray-700 shadow-lg";
  windowElement.style.width = "250px"; // Default width
  windowElement.style.height = "150px"; // Default height
  windowElement.style.zIndex = (highestZIndex++).toString();
  
  windowElement.innerHTML = `
    <div class="window-top bg-blue-500 text-white p-2 cursor-grab flex justify-between">
      <span>Drag Me</span>
      <button class="close-btn text-white font-bold px-2 hover:bg-red-500 hover:cursor-pointer">X</button>
    </div>
    <div class="window-content p-4">
      New Window
    </div>
  `;

  const WE = windowElement as WindowElement;
  WE.workspaceX = 50;
  WE.workspaceY = 50;
  WE.dragX = WE.workspaceX;
  WE.dragY = WE.workspaceY;
  WE.initialTopY = null;
  WE.initialBottomY = null;
  WE.minDimensions = { height: 100, width: 150 };
  WE.maxDimensions = undefined;

  // Set initial position and attributes
  WE.style.transform = `translate(${WE.workspaceX}px, ${WE.workspaceY}px)`;

  // Append to workspace
  WINDOW_MAP.set(WINDOW_ID, WE)
  WORKSPACE.appendChild(WE);  

  // Make draggable
  makeDraggable(WE);
  makeResizable(WE);
  addWindowEventHandlers(WE);
}

const doIntervalsOverlap = (start1: number, end1: number, start2: number, end2: number): boolean => {
  // https://nedbatchelder.com/blog/201310/range_overlap_in_two_compares.html
  return (end1 >= start2) && (end2 >= start1)
}

const DRAG_SNAP_THRESHOLD = 10;

const makeDraggable = (windowElement: WindowElement) => {
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
const makeResizable = (windowElement: WindowElement) => {
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

const makeResizable3 = (windowElement: WindowElement) => {
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

const addWindowEventHandlers = (windowElement: WindowElement) => {
  // Bring window to front when clicked
  windowElement.addEventListener("mousedown", () => {
    windowElement.style.zIndex = (highestZIndex++).toString();
  });

  // Close button functionality
  const closeButton = windowElement.querySelector(".close-btn") as HTMLButtonElement;
  closeButton.addEventListener("click", () => {
    WINDOW_MAP.delete(windowElement.id);
    windowElement.remove();
  });
};
