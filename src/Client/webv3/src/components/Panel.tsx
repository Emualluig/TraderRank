import { useEffect, useRef } from "react";
import interact from "interactjs";
import { useGlobalStore } from "../store";

interface PanelProps {
  id: string;
  title: string;
  workspaceRef: React.RefObject<HTMLDivElement | null>;
  children: React.ReactNode;
}

const DRAG_SNAP_THRESHOLD = 10;

const doIntervalsOverlap = (
  start1: number,
  end1: number,
  start2: number,
  end2: number
): boolean => {
  // https://nedbatchelder.com/blog/201310/range_overlap_in_two_compares.html
  return end1 >= start2 && end2 >= start1;
};

export function Panel({ id, title, workspaceRef, children }: PanelProps) {
  const panelRef = useRef<HTMLDivElement>(null);
  const layout = useGlobalStore.getState().panels[id];
  const bringToFront = useGlobalStore((s) => s.bringToFront);
  const setPanelSize = useGlobalStore((s) => s.setPanelSize);
  const setPanelPosition = useGlobalStore((s) => s.setPanelPosition);
  const setPanelDrag = useGlobalStore((s) => s.setPanelDrag);
  const removePanel = useGlobalStore((s) => s.removePanel);

  const MIN_SIZE = { height: 90, width: 160 } satisfies Interact.Size;
  const MAX_SIZE = { height: 500, width: 500 } satisfies Interact.Size;

  useEffect(() => {
    if (!panelRef.current) return;

    const interactable = interact(panelRef.current)
      .draggable({
        allowFrom: ".panel-title",
        modifiers: [
          interact.modifiers.restrictRect({
            restriction: "#workspace",
          }),
        ],
        listeners: {
          start: () => {
            const state = useGlobalStore.getState();
            const panels = state.panels;
            const current = panels[id];
            setPanelDrag(id, current.workspaceX, current.workspaceY);
          },
          move: (event: Interact.DragEvent) => {
            const state = useGlobalStore.getState();
            const panels = state.panels;
            const current = panels[id];
            if (!current) return;

            const workspaceRect = workspaceRef.current!.getBoundingClientRect();
            const workspaceRectX = workspaceRect.x;
            const workspaceRectY = workspaceRect.y;

            const target = event.target as HTMLElement;
            const selfRect = target.getBoundingClientRect();
            const selfTopY = selfRect.top;
            const selfBottomY = selfRect.bottom;
            const selfLeftX = selfRect.left;
            const selfRightX = selfRect.right;
            const selfWidth = selfRect.width;
            const selfHeight = selfRect.height;

            const dragX = current.dragX + event.dx;
            const dragY = current.dragY + event.dy;

            let finalX = dragX;
            let finalY = dragY;
            if (!event.shiftKey) {
              for (const [otherId, other] of Object.entries(panels)) {
                if (otherId === id) {
                  // Prevent self-collisions
                  continue;
                }
                const otherTopY = other.workspaceY + workspaceRectY;
                const otherBottomY = otherTopY + other.height;
                const otherLeftX = other.workspaceX + workspaceRectX;
                const otherRightX = otherLeftX + other.width;

                if (doIntervalsOverlap(otherTopY, otherBottomY, selfTopY, selfBottomY)) {
                  if (Math.abs(dragX - (otherRightX - workspaceRectX)) < DRAG_SNAP_THRESHOLD) {
                    // Snap the self left to the other right
                    finalX = otherRightX - workspaceRectX;
                  }
                  if (
                    Math.abs(dragX - (otherLeftX - selfWidth - workspaceRectX)) <
                    DRAG_SNAP_THRESHOLD
                  ) {
                    // snap the self right to the other left
                    finalX = otherLeftX - selfWidth - workspaceRectX;
                  }
                }
                if (doIntervalsOverlap(otherLeftX, otherRightX, selfLeftX, selfRightX)) {
                  if (Math.abs(dragY - (otherBottomY - workspaceRectY)) < DRAG_SNAP_THRESHOLD) {
                    // Snap the self top to the other bottom
                    finalY = otherBottomY - workspaceRectY;
                  }
                  if (
                    Math.abs(dragY - (otherTopY - selfHeight - workspaceRectY)) <
                    DRAG_SNAP_THRESHOLD
                  ) {
                    // Snap the self bottom to other top
                    finalY = otherTopY - selfHeight - workspaceRectY;
                  }
                }
              }
            }

            setPanelDrag(id, dragX, dragY);
            setPanelPosition(id, finalX, finalY);
          },
        },
      })
      .resizable({
        edges: { left: true, bottom: true, right: true },
        modifiers: [
          interact.modifiers.restrictEdges({
            outer: workspaceRef.current!,
          }),
          interact.modifiers.restrictSize({
            min: MIN_SIZE,
            max: MAX_SIZE,
          }),
        ],
        listeners: {
          start: () => {
            const state = useGlobalStore.getState();
            const panels = state.panels;
            const current = panels[id];
            setPanelDrag(id, current.width, current.height);
          },
          move: (event: Interact.ResizeEvent) => {
            // TODO: Add snapping to opposite side if self panel is edge to edge with
            // another edge
            const state = useGlobalStore.getState();
            const panels = state.panels;
            const current = panels[id];
            if (!current) return;

            const workspaceRect = workspaceRef.current!.getBoundingClientRect();
            const workspaceRectX = workspaceRect.x;
            const workspaceRectY = workspaceRect.y;

            const target = event.target as HTMLElement;
            const selfRect = target.getBoundingClientRect();
            const selfTopY = selfRect.top;
            const selfBottomY = selfRect.bottom;
            const selfLeftX = selfRect.left;
            const selfRightX = selfRect.right;

            const dragX = event.rect.width;
            const dragY = event.rect.height;

            let finalW = current.width + event.deltaRect!.width;
            let finalH = current.height + event.deltaRect!.height;
            let finalX = current.workspaceX;
            const finalY = current.workspaceY;
            if (!event.shiftKey) {
              for (const [otherId, other] of Object.entries(panels)) {
                if (otherId === id) {
                  // Prevent self-collision
                  continue;
                }
                const otherTopY = other.workspaceY + workspaceRectY;
                const otherBottomY = otherTopY + other.height;
                const otherLeftX = other.workspaceX + workspaceRectX;
                const otherRightX = otherLeftX + other.width;

                // Self left edge into other right edge
                if (
                  doIntervalsOverlap(otherTopY, otherBottomY, selfTopY, selfBottomY) &&
                  event.edges?.right &&
                  Math.abs(otherLeftX - selfLeftX - dragX) < DRAG_SNAP_THRESHOLD
                ) {
                  finalW = current.width + (otherLeftX - selfRightX);
                }

                // We are moving the left edge, shift the X to the left, we expand on the right edge
                // This anchors the right edge but moves the left
                if (event.edges?.left) {
                  finalX += event.delta.x;
                }
                // Self right edge into other left edge
                if (
                  doIntervalsOverlap(otherTopY, otherBottomY, selfTopY, selfBottomY) &&
                  event.edges?.left &&
                  Math.abs(selfRightX - dragX - otherRightX) < DRAG_SNAP_THRESHOLD
                ) {
                  finalX = otherRightX;
                  finalW = current.width + (selfLeftX - otherRightX);
                }

                // Self bottom into other top
                if (
                  doIntervalsOverlap(otherLeftX, otherRightX, selfLeftX, selfRightX) &&
                  event.edges?.bottom &&
                  Math.abs(selfTopY + dragY - otherTopY) < DRAG_SNAP_THRESHOLD
                ) {
                  finalH = current.height + (otherTopY - selfBottomY);
                }
              }
            }

            setPanelSize(id, finalW, finalH);
            setPanelPosition(id, finalX, finalY);
            setPanelDrag(id, dragX, dragY);
          },
        },
      });

    return () => {
      interactable.unset();
    };
  }, [id]);

  return (
    <div
      ref={panelRef}
      className='absolute bg-white border shadow-md flex flex-col select-none'
      style={{
        left: layout.workspaceX,
        top: layout.workspaceY,
        width: layout.width,
        height: layout.height,
        zIndex: layout.zIndex,
      }}
      onMouseDown={() => bringToFront(id)}
    >
      <div className='panel-title bg-blue-500 text-white p-2 cursor-grab flex justify-between items-center'>
        <span>{title}</span>
        <button
          className='close-btn text-white font-bold px-2 hover:bg-red-500 hover:cursor-pointer'
          onClick={() => removePanel(id)}
        >
          X
        </button>
      </div>
      <div className='panel-inner p-2 grow overflow-auto'>{children}</div>
    </div>
  );
}
