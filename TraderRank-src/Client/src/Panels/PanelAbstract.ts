import interact from 'interactjs';
import { IPanelElement, PanelElement } from './PanelElement';
import { PanelManager } from './PanelManager';

const DRAG_SNAP_THRESHOLD = 10;

const doIntervalsOverlap = (start1: number, end1: number, start2: number, end2: number): boolean => {
    // https://nedbatchelder.com/blog/201310/range_overlap_in_two_compares.html
    return (end1 >= start2) && (end2 >= start1)
}

const makeDraggable = (panelElement: PanelElement, manager: PanelManager) => {
    interact(panelElement).draggable({
        allowFrom: ".panel-title",
        modifiers: [
        interact.modifiers.restrictRect({
            restriction: manager.workspace,
        }),
        ],
        listeners: {
            start: (event: Interact.DragEvent) => {
                const target = event.target as PanelElement;
                target.dragX = target.workspaceX;
                target.dragY = target.workspaceY;
            },
            move: (event: Interact.DragEvent) => {
                const workspaceRect = manager.workspace.getBoundingClientRect();
                const workspaceX = workspaceRect.x;
                const workspaceY = workspaceRect.y;

                const target = event.target as PanelElement;
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
                    for (const otherWindow of manager.ValuesElements()) {
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

const makeResizable = (panelElement: PanelElement, manager: PanelManager) => {
    interact(panelElement).resizable({
        edges: { left: true, right: true,  bottom: true }, // Enable all edges
        modifiers: [
        interact.modifiers.restrictEdges({
            outer: manager.workspace, // Prevent resizing beyond workspace
        }),
        interact.modifiers.restrictSize({
            min: panelElement.minDimensions,
            max: panelElement.maxDimensions,
        }),
        ],
        listeners: {
            move: (event: Interact.ResizeEvent) => {
                const target = event.target as PanelElement;

                let x = target.workspaceX;
                let y = target.workspaceY;

                let { width, height } = event.rect; // New size from interact.js

                const workspaceRect = manager.workspace.getBoundingClientRect();

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

export abstract class PanelAbstract {
    private readonly _element: PanelElement;
    public get element(): PanelElement {
        return this._element;
    }
    private readonly _inner: HTMLDivElement;
    protected get inner(): HTMLDivElement {
        return this._inner;
    }
    private readonly _id: string;
    public get id(): string {
        return this._id;
    }

    public abstract Notify(obj: any): void;

    protected constructor(
        manager: PanelManager,
        title: string,
        minDimensions: Interact.Size,
        maxDimensions: Interact.Size | null,
    ) {
        this._id = `window_${crypto.randomUUID().slice(0, 8)}`;
        const panelProperties: IPanelElement = {
            workspaceX: 50,
            workspaceY: 50,
            dragX: 50,
            dragY: 50,
            initialTopY: null,
            initialBottomY: null,
            minDimensions: minDimensions,
            maxDimensions: maxDimensions ?? undefined,
            PanelRef: this
        };
        this._element = (Object.assign(document.createElement("div"), panelProperties) satisfies PanelElement) as PanelElement;
        this._element.id = this._id;
        this._element.style.width = `${minDimensions.width}px`;
        this._element.style.height = `${minDimensions.height}px`;
        this._element.style.zIndex = (manager.highestZIndex++).toString();
        this._element.style.transform = `translate(${this._element.workspaceX}px, ${this._element.workspaceY}px)`;
        this._element.className = "panel absolute bg-white border border-gray-700 shadow-lg flex flex-col";
        this._element.innerHTML = `
            <div class="panel-title bg-blue-500 text-white p-2 cursor-grab flex justify-between">
                <span>${title}</span>
                <button class="close-btn text-white font-bold px-2 hover:bg-red-500 hover:cursor-pointer">X</button>
            </div>
            <div class="panel-inner p-2 relative grow shrink"></div>
        `;
        const inner = this._element.querySelector("div.panel-inner");
        if (inner === null) {
            throw new Error("Invariant: Couldn't find `div.panel-inner`.");
        }
        this._inner = inner as HTMLDivElement;

        makeDraggable(this._element, manager);
        makeResizable(this._element, manager);
        manager.AppendPanel(this);

        // Bring window to front when clicked
        this._element.addEventListener("mousedown", () => {
            this._element.style.zIndex = (manager.highestZIndex++).toString();
        });

        // Close button functionality
        const closeButton = this._element.querySelector(".close-btn") as HTMLButtonElement;
        closeButton.addEventListener("click", () => {
            manager.DeletePanel(this);
        });
    }
}