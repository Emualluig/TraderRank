import { MarketState } from "../MarketHelpers";
import { PanelAbstract } from "./PanelAbstract";
import { PanelManager } from "./PanelManager";
import Chart from 'chart.js/auto';

export class PanelChart extends PanelAbstract {
    private readonly chart: Chart;
    public constructor(manager: PanelManager) {
        super(manager, "PanelChart", { height: 300, width: 250 }, null);
        this.inner.classList.add("overflow-hidden");
        
        const canvas = document.createElement("canvas");
        this.inner.appendChild(canvas);
        
        this.chart = new Chart(canvas, {
            type: 'line',
            data: {
                datasets: [{
                    label: 'Midpoint Price',
                    data: [],
                    borderColor: 'blue',
                    borderWidth: 2,
                    pointRadius: 0,
                    fill: false,
                    tension: 0.1,
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                scales: {
                    x: {
                        type: 'linear',
                        position: 'bottom',
                    },
                    y: {
                        beginAtZero: false,
                    }
                }
            }
        });
    }

    public Notify(ms: MarketState): void {
        this.chart.data.datasets[0].data = ms.midpointHistory.map(el => ({ x: el.timestamp, y: el.price }));
        this.chart.update();
    }
}