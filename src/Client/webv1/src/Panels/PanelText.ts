import { PanelAbstract } from "./PanelAbstract";
import { PanelManager } from "./PanelManager";

export class PanelText extends PanelAbstract {
    public constructor(manager: PanelManager) {
        super(manager, "PanelText", { height: 100, width: 250 }, null);
        const inner = this.inner;
        inner.innerText = "Hello!";
    }

    public Notify(obj: any): void {}
}