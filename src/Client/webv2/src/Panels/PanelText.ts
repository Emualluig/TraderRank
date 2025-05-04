import { WSMessageSync, WSMessageUpdate } from "../core";
import { PanelAbstract } from "./PanelAbstract";
import { PanelManager } from "./PanelManager";

export class PanelText extends PanelAbstract {
    public onSync(message: WSMessageSync): void {}
    public onUpdate(message: WSMessageUpdate): void {}

    public constructor(manager: PanelManager) {
        super(manager, "PanelText", { height: 100, width: 250 }, null);
        const inner = this.inner;
        inner.innerText = "Hello!";
    }
}