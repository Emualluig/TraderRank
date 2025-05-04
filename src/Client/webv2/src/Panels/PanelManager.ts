import { PanelAbstract, PanelElement } from "./PanelAbstract";

export class PanelManager {
    private readonly WORKSPACE: HTMLElement;
    private readonly ELEMENT_MAP: Map<string, PanelElement>;
    private readonly PANEL_MAP: Map<string, PanelAbstract>;

    public constructor(WORKSPACE: HTMLElement) {
        this.WORKSPACE = WORKSPACE;
        this.ELEMENT_MAP = new Map<string, PanelElement>();
        this.PANEL_MAP = new Map<string, PanelAbstract>();
    }
    public AppendPanel(panel: PanelAbstract) {
        const id = panel.id;
        const element = panel.element;
        this.ELEMENT_MAP.set(id, element);
        this.PANEL_MAP.set(id, panel);
        this.WORKSPACE.appendChild(element);
    }
    public DeletePanel(panel: PanelAbstract) {
        const id = panel.id;
        const element = panel.element;
        this.ELEMENT_MAP.delete(id);
        this.PANEL_MAP.delete(id);
        element.remove();
    }

    public highestZIndex: number = 0;

    public IterElements() {
        return this.ELEMENT_MAP.entries();
    }
    public ValuesElements() {
        return this.ELEMENT_MAP.values();
    }
    public IterPanels() {
        return this.PANEL_MAP.entries();
    }
    public ValuesPanel() {
        return this.PANEL_MAP.values();
    }
    public get workspace(): HTMLElement {
        return this.WORKSPACE;
    }
}