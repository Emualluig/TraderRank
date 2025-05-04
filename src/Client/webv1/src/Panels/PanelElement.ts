import { PanelAbstract } from "./PanelAbstract";

export interface PanelElement extends HTMLElement, IPanelElement {}

export interface IPanelElement {
    workspaceX: number;
    workspaceY: number;
    dragX: number;
    dragY: number;
    initialTopY: number | null;
    initialBottomY: number | null;
    minDimensions: Interact.Size | undefined;
    maxDimensions: Interact.Size | undefined;
    PanelRef: PanelAbstract
}
