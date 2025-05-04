import { LimitOrder, MarketUpdate } from "../types";
import { PanelAbstract } from "./PanelAbstract";
import { PanelManager } from "./PanelManager";

function difference<T>(setA: Set<T>, setB: Set<T>) {
    return new Set<T>([...setA].filter(x => !setB.has(x)));
}

export class PanelBook extends PanelAbstract {
    private tableBodyBids: HTMLTableSectionElement;
    private tableBodyAsks: HTMLTableSectionElement;
    private availableSecurities = new Set<string>();

    public constructor(manager: PanelManager) {
        super(manager, "PanelBook", { height: 300, width: 400 }, null);
        this.inner.classList.add("overflow-hidden", "flex", "flex-col", "gap-1");

        // Create table container
        const tableContainer = document.createElement("div");
        tableContainer.classList.add("flex", "w-full", "h-full", "overflow-hidden");

        // Create dropdown at top to select security
        const selectId = `select-${this.id}`;
        const selectName = `select-n-${this.id}`;
        const selectContainer = document.createElement("div");
        selectContainer.classList.add("w-full");
        const select = document.createElement("select");
        select.setAttribute("name", selectName);
        select.setAttribute("id", selectId);
        select.classList.add("border", "w-25");
        for (const optionText of ["BIOTECH", "CAD"]) {
            const option = document.createElement("option");
            option.value = optionText;
            option.label = optionText;
            select.append(option);
        }
        selectContainer.appendChild(select);
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

    public Notify(obj: MarketUpdate): void {
        
    }

    private createTable() {
        const tableWrapper = document.createElement("div");
        tableWrapper.classList.add("flex-grow", "overflow-y-auto", "border");

        const table = document.createElement("table");
        table.classList.add("w-full", "border-collapse");

        const thead = document.createElement("thead");
        thead.innerHTML = `<tr class="bg-gray-700 text-white sticky">
            <th>Quantity</th><th>Price</th>
        </tr>`;
        table.appendChild(thead);

        const tbody = document.createElement("tbody");
        table.appendChild(tbody);
        tableWrapper.appendChild(table);

        return { table: tableWrapper, tbody };
    }

    /*
    public Notify(ms: MarketState): void {
        const FLASH_TIME = 1_000;
        const REMOVE_COLOR_CLASS = "bg-red-300";
        const ADDED_COLOR_CLASS = "bg-green-300";
        if (ms.timestamp === 0) {
            this.currentBidMap = new Map();
            this.currentAskMap = new Map();
        }
        this.tableBodyBids.innerHTML = "";
        this.tableBodyAsks.innerHTML = "";

        const newBidIds = ms.currentBooks.bids.reduce((acc, curr) => {
            acc.add(curr.id);
            return acc;
        }, new Set<number>());
        const deletedBidsIds = difference(new Set(this.currentBidMap.keys()), newBidIds);
        const addedBidsIds = difference(newBidIds, new Set(this.currentBidMap.keys()));

        const nodesBids: [HTMLTableRowElement, LimitOrder][] = [];
        for (const order of ms.currentBooks.bids) {
            const tr = document.createElement("tr");
            if (addedBidsIds.has(order.id)) {
                tr.classList.add(ADDED_COLOR_CLASS);
            }
            tr.innerHTML = `
                <td>${order.quantity.toFixed(2)}</td>
                <td>${order.price.toFixed(2)}</td>
            `;
            nodesBids.push([tr, order]);
        }
        for (const orderId of deletedBidsIds) {
            const tr = document.createElement("tr");
            tr.classList.add(REMOVE_COLOR_CLASS);
            const order = this.currentBidMap.get(orderId)!;
            tr.innerHTML = `
                <td>${order.quantity.toFixed(2)}</td>
                <td>${order.price.toFixed(2)}</td>
            `;
            nodesBids.push([tr, order]);
        }

        const sortedBids = nodesBids.sort((a, b) => {
            const orderA = a[1];
            const orderB = b[1];
            if (orderA.price > orderB.price) {
                return -1;
            } else if (orderA.price < orderB.price) {
                return 1;
            } else {
                if (orderA.timestamp > orderB.timestamp) {
                    return 1;
                } else if (orderA.timestamp < orderB.timestamp) {
                    return -1;
                } else {
                    return orderA.id - orderB.id;
                }
            }
        }).map(el => el[0]);

        for (const tr of sortedBids) {
            this.tableBodyBids.appendChild(tr);
        }
        this.currentBidMap = ms.currentBooks.bids.reduce((acc, curr) => {
            acc.set(curr.id, curr);
            return acc;
        }, new Map<number, LimitOrder>());

        const newAskIds = ms.currentBooks.asks.reduce((acc, curr) => {
            acc.add(curr.id);
            return acc;
        }, new Set<number>());
        const deletedAsksIds = difference(new Set(this.currentAskMap.keys()), newAskIds);
        const addedAsksIds = difference(newAskIds, new Set(this.currentAskMap.keys()));

        const nodesAsks: [HTMLTableRowElement, LimitOrder][] = [];
        for (const order of ms.currentBooks.asks) {
            const tr = document.createElement("tr");
            if (addedAsksIds.has(order.id)) {
                tr.classList.add(ADDED_COLOR_CLASS);
            }
            tr.innerHTML = `
                <td>${order.quantity.toFixed(2)}</td>
                <td>${order.price.toFixed(2)}</td>
            `;
            nodesAsks.push([tr, order]);
        }
        for (const orderId of deletedAsksIds) {
            const tr = document.createElement("tr");
            tr.classList.add(REMOVE_COLOR_CLASS);
            const order = this.currentAskMap.get(orderId)!;
            tr.innerHTML = `
                <td>${order.quantity.toFixed(2)}</td>
                <td>${order.price.toFixed(2)}</td>
            `;
            nodesAsks.push([tr, order]);
        }

        const sortedAsks = nodesAsks.sort((a, b) => {
            const orderA = a[1];
            const orderB = b[1];
            if (orderA.price > orderB.price) {
                return 1;
            } else if (orderA.price < orderB.price) {
                return -1;
            } else {
                if (orderA.timestamp > orderB.timestamp) {
                    return 1;
                } else if (orderA.timestamp < orderB.timestamp) {
                    return -1;
                } else {
                    return orderA.id - orderB.id;
                }
            }
        }).map(el => el[0]);

        for (const tr of sortedAsks) {
            this.tableBodyAsks.appendChild(tr);
        }
        this.currentAskMap = ms.currentBooks.asks.reduce((acc, curr) => {
            acc.set(curr.id, curr);
            return acc;
        }, new Map<number, LimitOrder>());
    }

    private updateTable(orders: LimitOrder[], tableBody: HTMLTableSectionElement, isPriceFirst: boolean) {
        
        
        tableBody.innerHTML = "";
        for (const order of orders) {
            const tr = document.createElement("tr");
            tr.innerHTML = `
                <td>${isPriceFirst ? order.price : order.quantity}</td><td>${isPriceFirst ? order.quantity : order.price}</td>
            `;
            tableBody.appendChild(tr);
        }
    }
    */
}
