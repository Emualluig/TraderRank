import { GLOBAL_MARKET_STATE, OrderBook, WSMessageSync, WSMessageUpdate } from "../core";
import { PanelAbstract } from "./PanelAbstract";
import { PanelManager } from "./PanelManager";

function difference<T>(setA: Set<T>, setB: Set<T>) {
    return new Set<T>([...setA].filter(x => !setB.has(x)));
}

class SecuritySelectDropdown {
    private parent: PanelAbstract;
    private selectElement: HTMLSelectElement;

    public constructor(parent: PanelAbstract) {
        this.parent = parent;
        const select = document.createElement("select");
        select.setAttribute("name", `select-n-${this.parent.id}`);
        select.setAttribute("id", `select-${this.parent.id}`);
        select.classList.add("border", "w-25");
        this.selectElement = select;
        if (GLOBAL_MARKET_STATE.sync_message !== null) {
            this.setSecurityDropdown(GLOBAL_MARKET_STATE.sync_message);
        }
    }

    private setSecurityDropdown(message: WSMessageSync): void {
        this.selectElement.innerHTML = "";
        for (const tradable_id of message.tradable_securities_id) {
            const tradable_ticker = message.securities[tradable_id];
            const option = document.createElement("option");
            option.value = tradable_ticker;
            option.label = tradable_ticker;
            this.selectElement.appendChild(option);
        }
    }

    public onSync(message: WSMessageSync) {
        this.setSecurityDropdown(message);
    }

    public getSelectElement(): HTMLSelectElement {
        return this.selectElement;
    }

    public getSelectedTicker(): string {
        return this.selectElement.value;
    }
}

interface LimitOrderRow extends HTMLTableRowElement {
    order_id: number;
    price: number;
}

export class PanelBook extends PanelAbstract {
    private selector: SecuritySelectDropdown;
    private tableBodyBids: HTMLTableSectionElement;
    private tableBodyAsks: HTMLTableSectionElement;

    public constructor(manager: PanelManager) {
        super(manager, "PanelBook", { height: 300, width: 400 }, null);
        this.inner.classList.add("overflow-hidden", "flex", "flex-col", "gap-1");

        // Create table container
        const tableContainer = document.createElement("div");
        tableContainer.classList.add("flex", "w-full", "h-full", "overflow-hidden");

        // Create dropdown at top to select security
        const selectContainer = document.createElement("div");
        selectContainer.classList.add("w-full");

        // Create a security selector
        this.selector = new SecuritySelectDropdown(this);
        selectContainer.appendChild(this.selector.getSelectElement())

        // We currently don't have any options at this point so we just don't add anything
        this.inner.appendChild(selectContainer)

        // Create Bids Table
        const bidsTable = this.createTable();
        this.tableBodyBids = bidsTable.tbody;

        // Create Asks Table
        const asksTable = this.createTable();
        this.tableBodyAsks = asksTable.tbody;

        // Append tables to container
        tableContainer.appendChild(bidsTable.table);
        tableContainer.appendChild(asksTable.table);
        this.inner.appendChild(tableContainer);
    }

    private createTable() {
        const tableWrapper = document.createElement("div");
        tableWrapper.classList.add("flex-grow", "overflow-y-auto", "border");

        const table = document.createElement("table");
        table.classList.add("w-full", "border-collapse");

        const thead = document.createElement("thead");
        thead.innerHTML = `<tr class="bg-gray-700 text-white sticky top-0">
            <th>Quantity</th><th>Price</th>
        </tr>`;
        table.appendChild(thead);

        const tbody = document.createElement("tbody");
        table.appendChild(tbody);
        tableWrapper.appendChild(table);

        return { table: tableWrapper, tbody };
    }

    private oldBook: OrderBook|null = null;
    private bidOrders = new Map<number, LimitOrderRow>();
    public onSync(message: WSMessageSync): void {
        this.selector.onSync(message);
        const ticker = this.selector.getSelectedTicker();

        this.oldBook = message.order_book_per_security[ticker];
        this.bidOrders = new Map<number, LimitOrderRow>();
        this.tableBodyBids.innerHTML = "";

        const bids = message.order_book_per_security[ticker].bids;
        for (const b of bids) {
            const tr = document.createElement("tr") as LimitOrderRow;
            tr.innerHTML = `
                <td>${b.volume.toFixed(2)}</td>
                <td>${b.price.toFixed(2)}</td>
            `;
            tr.price = b.price;
            tr.order_id = b.order_id;
            this.bidOrders.set(b.order_id, tr);
            this.tableBodyBids.appendChild(tr);
        }
    }

    public onUpdate(message: WSMessageUpdate): void {
        const HAS_FLASH = true;
        const FLASH_TIME = 300;
        const REMOVE_COLOR_CLASS = "bg-red-300";
        const ADDED_COLOR_CLASS = "bg-green-300";

        const ticker = this.selector.getSelectedTicker();
        const oldBook = this.oldBook!;
        const newBook = message.order_book_per_security[ticker];
        this.oldBook = newBook;

        const oldBookBidIds = new Set(oldBook.bids.map(el => el.order_id));
        const newBookBidIds = new Set(newBook.bids.map(el => el.order_id));

        const removedBidIds = difference(oldBookBidIds, newBookBidIds);
        const addedBidsIds = difference(newBookBidIds, oldBookBidIds);

        for (const removedId of removedBidIds.values()) {
            const element = this.bidOrders.get(removedId);
            if (element !== undefined) {
                if (HAS_FLASH) {
                    element.classList.add(REMOVE_COLOR_CLASS);
                    setTimeout(() => {
                        element.remove();
                        this.bidOrders.delete(removedId);
                    }, FLASH_TIME);
                } else {
                    element.remove();
                    this.bidOrders.delete(removedId);
                }
            }
        }

        const addedOrders = newBook.bids.filter(el => addedBidsIds.has(el.order_id));
        let idx = 0;
        for (const child of this.tableBodyBids.children) {
            const j = child as LimitOrderRow;
            if (idx < addedOrders.length && j.price < addedOrders[idx].price) {
                const b = addedOrders[idx];
                const tr = document.createElement("tr") as LimitOrderRow;
                tr.innerHTML = `
                    <td>${b.volume.toFixed(2)}</td>
                    <td>${b.price.toFixed(2)}</td>
                `;
                if (HAS_FLASH) {
                    tr.classList.add(ADDED_COLOR_CLASS);
                    setTimeout(() => {
                        tr.classList.remove(ADDED_COLOR_CLASS);
                    }, FLASH_TIME);
                }
                tr.price = b.price;
                tr.order_id = b.order_id;
                this.bidOrders.set(b.order_id, tr);
                this.tableBodyBids.insertBefore(tr, j);
                idx += 1;
            }
        }
        for (let i = idx; i < addedOrders.length; i++) {
            const b = addedOrders[i];
            const tr = document.createElement("tr") as LimitOrderRow;
            tr.innerHTML = `
                <td>${b.volume.toFixed(2)}</td>
                <td>${b.price.toFixed(2)}</td>
            `;
            if (HAS_FLASH) {
                tr.classList.add(ADDED_COLOR_CLASS);
                setTimeout(() => {
                    tr.classList.remove(ADDED_COLOR_CLASS);
                }, FLASH_TIME);
            }
            tr.price = b.price;
            tr.order_id = b.order_id;
            this.bidOrders.set(b.order_id, tr);
            this.tableBodyBids.appendChild(tr);
        }

    }
}
