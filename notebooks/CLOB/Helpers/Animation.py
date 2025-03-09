from dataclasses import dataclass
from typing import Tuple
import matplotlib.pyplot as plt
import numpy as np
import matplotlib.animation as animation
from Helpers.SimulationAbstract import Simulation, SimulationStep

@dataclass
class AnimationOptions:
    figure_size: Tuple[float, float] | None
    show_order_book: bool
    show_midpoint: bool 
    show_volume: bool
    show_volatility: bool
    generate_filename: str | None
    fps: int | None
    show_plot: bool
    pass

def GenerateAnimation(simulation: Simulation, options: AnimationOptions):
    N = simulation.get_N()
    
    figure_size = options.figure_size or (8, 14)
    
    # Set up the figure dynamically based on options
    num_subplots = sum([options.show_order_book, options.show_midpoint, options.show_volume, options.show_volatility])
    fig, axes = plt.subplots(num_subplots, 1, figsize=figure_size, gridspec_kw={'height_ratios': [3] + [1] * (num_subplots - 1)})
    if num_subplots == 1:
        axes = [axes]  # Ensure axes is always a list
    
    plot_index = 0
    ax1 = ax2 = ax3 = ax4 = None
    
    if options.show_order_book:
        ax1 = axes[plot_index]
        ax1.set_xlim(85, 110)
        ax1.set_ylabel("Cumulative Volume")
        ax1.set_title("Order Book Depth")
        plot_index += 1
    
    if options.show_midpoint:
        ax2 = axes[plot_index]
        ax2.set_ylabel("Midpoint Price")
        ax2.set_title("Stock Market Midpoint")
        plot_index += 1
    
    if options.show_volume:
        ax3 = axes[plot_index]
        ax3.set_ylabel("Transacted Quantity")
        ax3.set_title("Transaction Volume")
        plot_index += 1
    
    if options.show_volatility:
        ax4 = axes[plot_index]
        ax4.set_ylabel("Rolling Volatility")
        ax4.set_title("Rolling 10-Frame Volatility")
        plot_index += 1
    
    # Initialize data storage
    midpoint_prices = []
    transaction_volumes = []
    rolling_volatility = []
    
    # Preallocate plot objects
    if ax1:
        bid_area = ax1.fill_between([], [], color='green', alpha=0.5)
        ask_area = ax1.fill_between([], [], color='red', alpha=0.5)
    if ax2:
        midpoint_line, = ax2.plot([], [], color='blue')
    if ax4:
        volatility_line, = ax4.plot([], [], color='orange')
    if ax3:
        volume_line, = ax3.plot([], [], color='purple')
    
    # Function to compute rolling volatility
    def compute_volatility(prices, window=10):
        if len(prices) < window:
            return 0
        return np.std(prices[-window:])
    
    first_call = True
    # Function to update animation frame
    def update(frame: int):
        nonlocal first_call
        if first_call:
            first_call = False
            return
        
        step = simulation.do_simulation_step()
        midpoint = (step.top_ask + step.top_bid) / 2
        midpoint_prices.append(midpoint)
        rolling_volatility.append(compute_volatility(midpoint_prices))
        transaction_quantity = sum(t.quantity for t in step.transactions)
        transaction_volumes.append(transaction_quantity)
        
        if ax2:
            midpoint_line.set_data(range(len(midpoint_prices)), midpoint_prices)
            ax2.relim()
            ax2.autoscale_view()
        
        if ax4:
            volatility_line.set_data(range(len(rolling_volatility)), rolling_volatility)
            ax4.relim()
            ax4.autoscale_view()
        
        if ax3:
            volume_line.set_data(range(len(transaction_volumes)), transaction_volumes)
            ax3.relim()
            ax3.autoscale_view()
        
        if ax1:
            depths = step.depths
            
            bid_prices = list(depths[0].keys())
            bid_volumes = list(depths[0].values())
            ask_prices = list(depths[1].keys())
            ask_volumes = list(depths[1].values())
            
            for collection in ax1.collections:
                collection.remove()
            
            ax1.fill_between(bid_prices, bid_volumes, color='green', alpha=0.5)
            ax1.fill_between(ask_prices, ask_volumes, color='red', alpha=0.5)
            ax1.set_xlim(min(min(bid_prices), min(ask_prices)), max(max(bid_prices), max(ask_prices)))
            ax1.set_ylim(0, max(bid_volumes + ask_volumes, default=50))  # Adjust dynamically
            ax1.set_ylabel("Cumulative Volume")
            ax1.set_title(f"Order Book Depth (Tick {frame}) (Transactions={len(step.transactions):>3})")
    
    # Create animation
    ani = animation.FuncAnimation(fig, update, frames=N, repeat=False, interval=10)
    
    if options.generate_filename:
        ani.save(options.generate_filename, fps=options.fps or 15, extra_args=['-vcodec', 'libx264'])
    
    if options.show_plot:
        plt.show()
    pass
