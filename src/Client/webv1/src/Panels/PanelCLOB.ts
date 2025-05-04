import { MarketState } from "../types";
import { PanelAbstract } from "./PanelAbstract";
import { PanelManager } from "./PanelManager";
import Chart from 'chart.js/auto';

export class PanelCLOB extends PanelAbstract {
    private readonly chart: Chart;

    public constructor(manager: PanelManager) {
        super(manager, "PanelCLOB", { height: 300, width: 250 }, null);
        this.inner.classList.add("overflow-hidden");

        const canvas = document.createElement("canvas");
        this.inner.appendChild(canvas);

        this.chart = new Chart(canvas, {
            type: 'line',
            data: {
                datasets: [
                    {
                        label: 'Bids',
                        data: [],
                        borderColor: 'green',
                        backgroundColor: 'rgba(0, 255, 0, 0.3)',
                        borderWidth: 2,
                        pointRadius: 0,
                        fill: true,
                        stepped: true, // Creates a "stair-step" effect like a real order book
                    },
                    {
                        label: 'Asks',
                        data: [],
                        borderColor: 'red',
                        backgroundColor: 'rgba(255, 0, 0, 0.3)',
                        borderWidth: 2,
                        pointRadius: 0,
                        fill: true,
                        stepped: true,
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false, // Disable animations
                scales: {
                    x: {
                        type: 'linear',
                        position: 'bottom',
                        title: {
                            display: true,
                            text: "Price Levels"
                        }
                    },
                    y: {
                        beginAtZero: true,
                        title: {
                            display: true,
                            text: "Order Volume"
                        }
                    }
                }
            }
        });
    }

    public Notify(ms: MarketState): void {
        // Convert market state to bid/ask depth chart format
        const bids = [...ms.currentDepths.bids.entries()].map(el => ({ x: el[0], y: el[1] }));
        const asks = [...ms.currentDepths.asks.entries()].map(el => ({ x: el[0], y: el[1] }));

        this.chart.data.datasets[0].data = bids; // Bids (green)
        this.chart.data.datasets[1].data = asks; // Asks (red)

        this.chart.update('none'); // Update without animation
    }
}
