import asyncio
from dataclasses import dataclass
import dataclasses
import json
from typing import Any, List, Tuple, Union
import websockets
from websockets import WebSocketServerProtocol
import numpy as np
import python_modules.Server as Server
from abc import ABC, abstractmethod
from threading import Thread, Lock
from enum import Enum

def volatility_strength_func(tick: int, t: float, dt: float):
    if 0 <= tick < 200:
        return 0.5
    elif 200 <= tick < 400:
        return 1.0
    elif 400 <= tick < 500:
        return 2.5
    elif 500 <= tick < 800:
        return 1.0
    elif 800 <= tick < 900:
        return 2.5
    else:
        return 0.5

def reversion_strength_func(tick: int, t: float, dt: float):
    return 100

def leaky_reversion_strength_func(tick: int, t: float, dt: float):
    return 10

def get_base_path(
    initial_price: float,
    up_price: float,
    down_price: float,
    total_ticks: int,
    extra_ticks: int,
    preliminary_probability: float,
    fda_probability: float,
    rng: np.random.Generator,
):
    had_good_preliminary_results = rng.uniform(0, 1.0) < preliminary_probability
    had_fda_accepted = rng.uniform(0, 1.0) < fda_probability if had_good_preliminary_results else rng.uniform(0, 1.0) > fda_probability
    base_bath = initial_price * np.ones(shape=(total_ticks + extra_ticks))
    base_bath[500:900] = (
        fda_probability * up_price + (1-fda_probability) * down_price 
            if had_good_preliminary_results else 
        (1-fda_probability) * up_price + fda_probability * down_price
    )
    base_bath[900:] = up_price if had_fda_accepted else down_price
    return base_bath, had_good_preliminary_results, had_fda_accepted

positive_preliminary_blurbs = [
    "BIOTECH announces promising preliminary Phase III trial results for its flagship drug Xeronex. Early data suggests significant efficacy improvements over existing treatments, with a strong safety profile. The company is preparing its FDA submission.",
    "BIOTECH reports early success in Xeronex trials. Patients in the trial group exhibited a marked improvement over placebo, with minimal adverse effects noted.",
    "Xeronex shows early promise: BIOTECH's lead candidate surpassed expectations in efficacy metrics. Investors hopeful for FDA green light.",
    "Strong preliminary data boosts BIOTECH outlook. Internal sources say response rates “far exceeded baseline”, with low dropout rates.",
]
negative_preliminary_blurbs = [
    "BIOTECH releases preliminary results of its Xeronex trial. While some efficacy was observed, the overall results fell short of expectations. Concerns remain about the statistical strength and side effects profile.",
    "Initial trial data for Xeronex underwhelms. While some therapeutic effects observed, results fall short of benchmarks.",
    "BIOTECH's Xeronex stumbles in early findings. Analysts cite “inconclusive efficacy” and “uncertain path forward.”",
    "Concerns mount as Xeronex fails to meet key trial endpoints. Company shares dip as confidence wavers.",
]
positive_fda_blurbs = [
    "The FDA has approved BIOTECH’s new drug Xeronex for market release. Analysts expect a major boost to the company’s revenues as it becomes the first therapy of its kind to reach commercial availability.",
    "FDA gives green light to BIOTECH’s Xeronex. Approval positions company as a front-runner in new therapeutics.",
    "Historic day for BIOTECH: Xeronex approved for use in the U.S. Market analysts expect blockbuster revenue potential.",
    "Regulatory win: FDA endorses Xeronex after thorough review. CEO cites “relentless innovation” and patient-focused development.",
]
negative_fda_blurbs = [
    "The FDA has rejected BIOTECH’s application for Xeronex. The agency cited concerns over insufficient efficacy and unresolved safety issues in the final submission package.",
    "FDA turns down Xeronex application, citing data inconsistencies and safety concerns. BIOTECH expected to revise and resubmit.",
    "BIOTECH setback: FDA rejects Xeronex. Company vows to conduct additional studies and address regulator concerns.",
    "Approval hopes dashed as Xeronex fails to secure FDA clearance. “Disappointing but not surprising,” says one analyst.",
]

class SimulationBiotech:
    def __init__(self):
        # Preliminary result | FDA Decision | Probability | Final Price
        # GOOD (50%) ($125)  | ACCEPT (75%) | 37.5%       | $150
        # GOOD (50%) ($125)  | REJECT (25%) | 12.5%       | $50
        # BAD  (50%) ($75)   | ACCEPT (25%) | 12.5%       | $150
        # BAD  (50%) ($75)   | REJECT (75%) | 37.5%       | $50
        #
        # Start price at $100
        # from step 0 to 200, random drift
        # from step 200 to 400 increased volatility
        # from 400 to 500, increased volatility
        # At 500, get preliminary results
        # from 500 to 800, decreased volatility
        # from 800 to 900, increased volatility
        # At 900, FDA releases decision
        # From 900 to 1000 convergence to true price (either 50 or 150), low volatility
        
        self.lock = Lock()
        self.rng = np.random.default_rng()
        
        self.total_steps = 1_000
        self.extra_steps = 100
        self.currency = Server.GenericSecurities.GenericCurrency("CAD")
        self.stock = Server.GenericSecurities.GenericStock("BIOTECH", "CAD")
        self.simulation = Server.GenericSimulation(
            { "CAD": self.currency, "BIOTECH": self.stock }, 1.0, self.total_steps
        )
        self.currency_id = self.simulation.get_security_id("CAD")
        self.stock_id = self.simulation.get_security_id("BIOTECH")
        self.anon_id = self.simulation.add_user("AGENT")
        
        self.initial_price = 100.0
        self.up_price = 150.0
        self.down_price = 50.0
        
        self.reset()
        pass
    
    def register_user(self, username: str) -> int:
        return self.simulation.add_user(username)
    
    async def handle_message(self, client_id: str, message: str):
        print(f"[Simulation] Received from {client_id}: {message}")
        pass
    
    async def step(self):
        done = False
        with self.lock:
            tick = self.simulation.get_tick()
            print(f"On tick: {tick}")
            t = self.simulation.get_t()
            dt = self.simulation.get_dt()
            volatility = volatility_strength_func(tick, t, dt)
            target = self.base_path[tick]
            future_target = self.base_path[tick + self.extra_steps]
            reversion = reversion_strength_func(tick, t, dt)
            leaky_reversion = leaky_reversion_strength_func(tick, t, dt)
            SPREAD = 0.02
            ORDER_SIZE_MIN = 1
            ORDER_SIZE_MAX = 25
            
            if tick == 0:
                assert self.simulation.get_ask_count(self.stock_id) + self.simulation.get_ask_count(self.stock_id) == 0
                
                price_distribution = self.rng.uniform(0.75, 1.0, size=50)
                volume_distribution = self.rng.integers(ORDER_SIZE_MIN, ORDER_SIZE_MAX, 50)

                bid_top_price = target - SPREAD
                bid_bottom_price = bid_top_price - 0.1 * volatility * bid_top_price
                bid_prices: List[float] = list(bid_top_price * price_distribution + bid_bottom_price * (1.0 - price_distribution))
                bids = [(Server.OrderSide.BID, price, volume_distribution[i]) for i, price in enumerate(bid_prices)]
                
                ask_bottom_price = target + SPREAD
                ask_top_price = ask_bottom_price + 0.1 * volatility * ask_bottom_price
                ask_prices: List[float] = list(ask_bottom_price * price_distribution + ask_top_price * (1.0 - price_distribution))
                asks = [(Server.OrderSide.ASK, price, volume_distribution[i]) for i, price in enumerate(ask_prices)]

                combined_orders: List[Tuple[Server.OrderSide, float, float]] = bids + asks
                self.rng.shuffle(combined_orders)

                for (side, price, volume) in combined_orders:
                    self.simulation.direct_insert_limit_order(self.anon_id, self.stock_id, side, round(price, 2), volume)
                    pass
                pass
            else:
                order_count = 5
                
                orders_ANON = list(self.simulation.get_all_open_user_orders(self.anon_id, self.stock_id))
                # Remove some percent of ANON orders
                if (k := int(len(orders_ANON) * self.removal_percentage)) > 0:
                    orders_to_remove = list(self.rng.choice(a=orders_ANON, size=k, replace=False))
                    for order_id in orders_to_remove:
                        self.simulation.submit_cancel_order(self.anon_id, self.stock_id, order_id)
                        pass
                    pass
                
                if self.simulation.get_bid_count(self.stock_id) > 0 and self.simulation.get_ask_count(self.stock_id) > 0:
                    top_bid_price = self.simulation.get_top_bid(self.stock_id).price
                    top_ask_price = self.simulation.get_top_ask(self.stock_id).price
                elif self.simulation.get_bid_count(self.stock_id) > 0:
                    top_bid_price = self.simulation.get_top_bid(self.stock_id).price
                    top_ask_price = top_bid_price + 0.5
                elif self.simulation.get_ask_count(self.stock_id) > 0:
                    top_ask_price = self.simulation.get_top_ask(self.stock_id).price
                    top_bid_price = top_ask_price + 0.5
                else:
                    midpoint = self.stock_midpoints[-1]
                    top_bid_price = midpoint + 0.5
                    top_ask_price = midpoint + 0.5
                
                bid_prices = (
                    top_bid_price - SPREAD
                    + reversion * (target - top_bid_price) * dt
                    + leaky_reversion * (future_target - top_bid_price) * dt
                    + volatility * np.sqrt(top_bid_price * dt)
                    * self.rng.normal(loc=0.0, scale=1.0, size=order_count)
                )
                bid_quantities = self.rng.integers(ORDER_SIZE_MIN, ORDER_SIZE_MAX, size=order_count)
                bids = [(Server.OrderSide.BID, price, bid_quantities[i]) for i, price in enumerate(bid_prices)]
                
                ask_prices = (
                    top_bid_price + SPREAD
                    + reversion * (target - top_ask_price) * dt
                    + leaky_reversion * (future_target - top_ask_price) * dt
                    + volatility * np.sqrt(top_ask_price * dt)
                    * self.rng.normal(loc=0.0, scale=1.0, size=order_count)
                )
                ask_quantities = self.rng.integers(ORDER_SIZE_MIN, ORDER_SIZE_MAX, size=order_count) 
                asks = [(Server.OrderSide.ASK, price, ask_quantities[i]) for i, price in enumerate(ask_prices)]
                
                combined_orders: List[Tuple[Server.OrderSide, float, float]] = bids + asks
                self.rng.shuffle(combined_orders)

                for (side, price, volume) in combined_orders:
                    self.simulation.submit_limit_order(self.anon_id, self.stock_id, side, round(price, 2), volume)
                    pass
                pass
            
            results = self.simulation.do_simulation_step()
            
            if self.simulation.get_bid_count(self.stock_id) > 0 and self.simulation.get_ask_count(self.stock_id) > 0:
                top_bid_price = self.simulation.get_top_bid(self.stock_id).price
                top_ask_price = self.simulation.get_top_ask(self.stock_id).price
            elif self.simulation.get_bid_count(self.stock_id) > 0:
                top_bid_price = self.simulation.get_top_bid(self.stock_id).price
                top_ask_price = top_bid_price + 0.5
            elif self.simulation.get_ask_count(self.stock_id) > 0:
                top_ask_price = self.simulation.get_top_ask(self.stock_id).price
                top_bid_price = top_ask_price + 0.5
            else:
                top_bid_price = self.stock_midpoints[-1] - 0.5
                top_ask_price = self.stock_midpoints[-1] + 0.5
            self.stock_midpoints.append((top_bid_price + top_ask_price) / 2.0)
            
            done = not results.has_next_step
                
            def convert(input: List[Server.LimitOrder]) -> List[dict[str, Any]]:
                return [{ "order_id": x.order_id, "price": x.price, "user_id": x.user_id, "volume": x.volume } for x in input]
            
            retobj = {
                "step": results.current_step,
                "cancelled_orders": {security: list(order_ids) for security, order_ids in results.cancelled_orders.items()},
                "partially_transacted_orders": {security: list(order_ids) for security, order_ids in results.partially_transacted_orders.items()},
                "fully_transacted_orders": {security: list(order_ids) for security, order_ids in results.fully_transacted_orders.items()},
                "portfolios": results.portfolios,
                "user_id_to_username_map": results.user_id_to_username_map,
                "order_book_per_security": {security: { "bids": convert(order_book[0]), "asks": convert(order_book[1]) } for security, order_book in results.order_book_per_security.items()}
            }
            await broadcast(json.dumps(retobj))
            pass
        if done:
            self.reset()
        pass
    
    def reset(self):
        with self.lock:
            self.simulation.reset_simulation()
            base_path, had_good_preliminary, had_good_fda = get_base_path(
                initial_price=self.initial_price, 
                up_price=self.up_price, 
                down_price=self.down_price, 
                total_ticks=self.total_steps, 
                extra_ticks=self.extra_steps + 1, 
                preliminary_probability=0.5, 
                fda_probability=0.75, 
                rng=self.rng
            )
            self.base_path = base_path
            
            self.news: dict[int, Tuple[int, str]] = {}
            self.news[500] = self.rng.choice(positive_preliminary_blurbs) if had_good_preliminary else self.rng.choice(negative_preliminary_blurbs)
            self.news[900] = self.rng.choice(positive_fda_blurbs) if had_good_fda else self.rng.choice(negative_fda_blurbs)
            
            self.stock_midpoints: List[float] = []
            self.removal_percentage = 0.1
            pass
        pass
    
    pass

# Shared global state
clients: dict[str, WebSocketServerProtocol] = {}  # client_id -> websocket
client_to_user_id: dict[str, int] = {}
simulation = SimulationBiotech()
simulation_running = False
simulation_task = None

@dataclass
class SyncMessage:
    type_ = "sync"
    client_id: int
    securities: list[str]
    security_to_id: dict[str, int]
    tradable_securities_id: list[int]
    pass

@dataclass
class BidAskBook:
    bids: list[Server.LimitOrder]
    asks: list[Server.LimitOrder]
    pass

@dataclass
class UpdateMessage:
    type_ = "update"
    step: int
    cancelled_orders: dict[str, list[int]]
    fully_transacted_orders: dict[str, list[int]]
    order_book_per_security: dict[str, BidAskBook]
    partially_transacted_orders: dict[str, list[int]]
    portfolios: list[list[float]]
    user_id_to_username_map: dict[int, str]
    pass

class EnhancedJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        return super().default(o)

async def broadcast(message: Union[SyncMessage, UpdateMessage]):
    for client in clients.values():
        await client.send(json.dumps(message, cls=EnhancedJSONEncoder))

async def step_loop():
    while True:
        if simulation_running:
            try:
                await simulation.step()
            except Exception as e:
                print(f"[STEP LOOP] Encountered exception: {e}") 
        await asyncio.sleep(0.25)

async def handle_client(websocket: WebSocketServerProtocol, path: str):
    client_id = str(id(websocket))
    clients[client_id] = websocket
    client_to_user_id[client_id] = simulation.register_user(client_id)
    print(f"[Server] Client {client_id} connected.")
    
    try:
        await websocket.send(json.dumps({ "client_id": client_id }))
        
        async for message in websocket:
            if simulation_running:
                await simulation.handle_message(client_id, message)
    except websockets.exceptions.ConnectionClosed:
        print(f"[Server] Client {client_id} disconnected.")
    except Exception as e:
        print(f"[Server] Received exception from {client_id}: {e}")
    finally:
        del clients[client_id]

async def start_command():
    global simulation_running
    if not simulation_running:
        simulation_running = True
        print("[Server] Simulation started.")
    else:
        print("[Server] Simulation is already running.")
    pass

async def pause_command():
    global simulation_running
    if simulation_running:
        simulation_running = False
        print("[Server] Simulation paused.")
    else:
        print("[Server] Simulation is not running.")
    pass

async def users_command():
    pass

async def terminal_loop():
    loop = asyncio.get_running_loop()
    while True:
        command = await loop.run_in_executor(None, input, ">>> ")
        command = command.strip().lower()
        match command:
            case "start":
                await start_command()
                pass
            case "pause":
                await pause_command()
                pass
            case "users":
                await users_command()
                pass
            case _:
                print("[Server] Unknown command.")
                pass

async def main():
    server = await websockets.serve(handle_client, "localhost", 8765)
    print("[Server] WebSocket server started at ws://localhost:8765")
    await asyncio.gather(
        terminal_loop(),
        step_loop(),
    )

if __name__ == "__main__":
    asyncio.run(main())
