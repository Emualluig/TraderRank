from DSA import SimulationConst, BlackScholesFinalPrices
import numpy as np
import multiprocessing as mp

def run_simulation(
    seed: int
):
    initial_bid_orders = [(99.0,  55.0), (98.0,  50.0), (97.0,  25.0), (97.0,  25.0), (97.0,  25.0)]
    initial_ask_orders = [(101.0, 55.0), (102.0, 50.0), (103.0, 25.0), (103.0, 25.0), (103.0, 25.0)]
    
    rng = np.random.default_rng(seed)
    N = 252
    simulation = SimulationConst(
        r=0.05, 
        sigma=0.2, 
        T=1.0, 
        N=N, 
        removal_percentage=0.1,
        bid_count_fn=lambda rng: rng.integers(0, 50),
        ask_count_fn=lambda rng: rng.integers(0, 50),
        initial_bid_price=99.0,
        initial_ask_price=101.0,
        rng=rng,
        initial_bid_orders=initial_bid_orders,
        initial_ask_orders=initial_ask_orders,
    )
    
    midpoint_price = None
    for _ in range(N):
        step = simulation.do_simulation_step()
        midpoint_price = (step.top_ask + step.top_bid) / 2
    
    return midpoint_price

# Function to run multiple simulations in parallel
def run_parallel_simulations(num_simulations: int, num_workers: int):
    with mp.Pool(processes=num_workers) as pool:
        seeds = np.random.randint(0, 999999, size=num_simulations)
        results = pool.map(run_simulation, seeds)
    return results

def BSM_calculations(num_simulations: int, r: float, sigma: float):
    return BlackScholesFinalPrices(100.0, r, sigma, 1.0, num_simulations, np.random.default_rng())

if __name__ == "__main__":
    num_simulations = 1_000
    num_workers = 8  # Adjust based on CPU cores
    final_prices_sim = run_parallel_simulations(
        num_simulations, num_workers,
    )
    
    final_prices_bsm = BSM_calculations(num_simulations, r=0.05, sigma=0.2)

    # Print summary statistics
    print(f"Sim Mean Final Price: {np.mean(final_prices_sim):.2f}")
    print(f"Sim Std  Final Price: {np.std(final_prices_sim):.2f}")
    
    print(f"BSM Mean Final Price: {np.mean(final_prices_bsm):.2f}")
    print(f"BSM Std  Final Price: {np.std(final_prices_bsm):.2f}")
    
    