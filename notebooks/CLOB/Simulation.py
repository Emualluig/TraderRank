from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Callable, List, Tuple
import numpy as np
from Helpers.Classes import LimitOrder, Transaction
from Helpers.CLOB import CLOB


@dataclass
class SimulationStep:
    transactions: List[Transaction]
    depths: tuple[dict[float, float], dict[float, float]]
    top_bid: float
    top_ask: float
    pass


class Simulation(ABC):
    @abstractmethod
    def do_simulation_step(self) -> SimulationStep: ...
    
    pass

class SimulationConst(Simulation):
    clob: CLOB
    r: float
    sigma: float
    T: float
    N: float
    removal_percentage: float
    rng: np.random.Generator
    initial_bid_price: float
    initial_ask_price: float
    bid_count_fn: Callable[[], int]
    ask_count_fn: Callable[[], int]
    _action_count: int
    _timestamp: int
    _generate_deep_initial_order_book: bool
    _is_first_loop: bool
    _protected_ids: set[int]
    
    def __init__(self, 
                 r: float, 
                 sigma: float, 
                 T: float, 
                 N: int, 
                 removal_percentage: float,
                 initial_bid_price: float,
                 initial_ask_price: float,
                 bid_count_fn: Callable[[np.random.Generator], int],
                 ask_count_fn: Callable[[np.random.Generator], int],
                 rng: np.random.Generator,
                 initial_bid_orders: List[Tuple[float, float]],
                 initial_ask_orders: List[Tuple[float, float]]
                ):
        self.clob = CLOB()
        self.r = r
        self.sigma = sigma
        self.T = T
        self.N = N
        self.removal_percentage = removal_percentage
        self.rng = rng
        self.initial_bid_price = initial_bid_price
        self.initial_ask_price = initial_ask_price
        self.bid_count_fn = bid_count_fn
        self.ask_count_fn = ask_count_fn
        self._action_count = 0
        self._timestamp = 0
        self._protected_ids = set()
        
        for initial_bid in initial_bid_orders:
            self.clob.submit_bid(LimitOrder(price=initial_bid[0], timestamp=0, quantity=initial_bid[1], id=self._action_count))
            # self._protected_ids.add(self._action_count)
            self._action_count += 1
        
        for initial_ask in initial_ask_orders:
            self.clob.submit_ask(LimitOrder(price=initial_ask[0], timestamp=0, quantity=initial_ask[1], id=self._action_count))
            # self._protected_ids.add(self._action_count)
            self._action_count += 1

    def _compute_volatility(prices: List[float], window: int):
        if len(prices) < window:
            return np.nan
        return np.std(prices[-window:])
    
    def do_simulation_step(self) -> SimulationStep:
        if self._timestamp > self.N:
            raise Exception("Simulation is finished")
        self._timestamp += 1
        
        # Remove some percent of random orders
        bid_ids = self.clob.get_all_bid_ids()        
        if (k := int(len(bid_ids) * self.removal_percentage)) > 0:
            bids_to_remove = list(self.rng.choice(a=bid_ids, size=k, replace=False))
            for bid_id in bids_to_remove:
                if bid_id in self._protected_ids:
                    continue
                self.clob.cancel_bid(bid_id)        
            
        ask_ids = self.clob.get_all_ask_ids()
        if (k := int(len(ask_ids) * self.removal_percentage)) > 0:            
            asks_to_remove = list(self.rng.choice(a=ask_ids, size=k, replace=False))
            for ask_id in asks_to_remove:
                if ask_id in self._protected_ids:
                    continue
                self.clob.cancel_ask(ask_id)
        
        top_bid = self.clob.top_bid()
        top_ask = self.clob.top_ask()
        
        if top_bid == None and top_ask == None:
            top_bid_price = self.initial_bid_price
            top_ask_price = self.initial_ask_price
        elif top_bid == None and top_ask != None:
            top_bid_price = top_ask.price - 0.04
            top_ask_price = top_ask.price
        elif top_bid != None and top_ask == None:
            top_bid_price = top_bid.price
            top_ask_price = top_bid.price + 0.04
        else:
            top_bid_price = top_bid.price
            top_ask_price = top_ask.price
            
        delta = self.T / self.N
           
        bid_count = self.bid_count_fn(self.rng)
        bid_Z = self.rng.normal(loc=0, scale=1.0, size=bid_count)
        bid_prices = top_bid_price * np.exp((self.r - 0.5 * self.sigma**2) * delta + self.sigma * np.sqrt(delta) * bid_Z)
        bid_volumes = self.rng.integers(1, 5, size=bid_count)
        
        ask_count = self.ask_count_fn(self.rng)
        ask_Z = self.rng.normal(loc=0, scale=1.0, size=ask_count)
        ask_prices = top_ask_price * np.exp((self.r - 0.5 * self.sigma**2) * delta + self.sigma * np.sqrt(delta) * ask_Z)
        ask_volumes = self.rng.integers(1, 5, size=ask_count)
        
        for j in range(bid_count):
            self.clob.submit_bid(LimitOrder(
                price=round(bid_prices[j], 2), 
                timestamp=self._timestamp, 
                quantity=bid_volumes[j], 
                id=self._action_count
            ))
            self._action_count += 1
            
        for j in range(ask_count):
            self.clob.submit_ask(LimitOrder(
                price=round(ask_prices[j], 2), 
                timestamp=self._timestamp, 
                quantity=ask_volumes[j], 
                id=self._action_count
            ))
            self._action_count += 1
        
        transactions = self.clob.process_transactions()
        depths = self.clob.get_cumulative_depth()
        
        return SimulationStep(
            transactions=transactions,
            depths=depths,
            top_bid=top_bid_price,
            top_ask=top_ask_price
        )
    pass


class SimulationTargeting(Simulation):
    clob: CLOB
    r: float
    sigma: float
    T: float
    N: float
    removal_percentage: float
    rng: np.random.Generator
    initial_bid_price: float
    initial_ask_price: float
    bid_count_fn: Callable[[], int]
    ask_count_fn: Callable[[], int]
    _action_count: int
    _timestamp: int
    
    def __init__(self, 
                 r: float, 
                 sigma: float, 
                 T: float, 
                 N: int, 
                 removal_percentage: float,
                 initial_bid_price: float,
                 initial_ask_price: float,
                 bid_count_fn: Callable[[np.random.Generator], int],
                 ask_count_fn: Callable[[np.random.Generator], int],
                 rng: np.random.Generator,
                 initial_bid_orders: List[Tuple[float, float]],
                 initial_ask_orders: List[Tuple[float, float]]
                ):
        self.clob = CLOB()
        self.r = r
        self.sigma = sigma
        self.T = T
        self.N = N
        self.removal_percentage = removal_percentage
        self.rng = rng
        self.initial_bid_price = initial_bid_price
        self.initial_ask_price = initial_ask_price
        self.bid_count_fn = bid_count_fn
        self.ask_count_fn = ask_count_fn
        self._action_count = 0
        self._timestamp = 0
        
        for initial_bid in initial_bid_orders:
            self.clob.submit_bid(LimitOrder(price=initial_bid[0], timestamp=0, quantity=initial_bid[1], id=self._action_count))
            self._action_count += 1
        
        for initial_ask in initial_ask_orders:
            self.clob.submit_ask(LimitOrder(price=initial_ask[0], timestamp=0, quantity=initial_ask[1], id=self._action_count))
            self._action_count += 1
    
    def do_simulation_step(self) -> SimulationStep:
        if self._timestamp > self.N:
            raise Exception("Simulation is finished")
        self._timestamp += 1
        
        # Remove some percent of random orders
        bid_ids = self.clob.get_all_bid_ids()        
        if (k := int(len(bid_ids) * self.removal_percentage)) > 0:
            bids_to_remove = list(self.rng.choice(a=bid_ids, size=k, replace=False))
            for bid_id in bids_to_remove:
                self.clob.cancel_bid(bid_id)        
            
        ask_ids = self.clob.get_all_ask_ids()
        if (k := int(len(ask_ids) * self.removal_percentage)) > 0:            
            asks_to_remove = list(self.rng.choice(a=ask_ids, size=k, replace=False))
            for ask_id in asks_to_remove:
                self.clob.cancel_ask(ask_id)
        
        top_bid = self.clob.top_bid()
        top_ask = self.clob.top_ask()
        
        if top_bid == None and top_ask == None:
            top_bid_price = self.initial_bid_price
            top_ask_price = self.initial_ask_price
        elif top_bid == None and top_ask != None:
            top_bid_price = top_ask.price - 0.04
            top_ask_price = top_ask.price
        elif top_bid != None and top_ask == None:
            top_bid_price = top_bid.price
            top_ask_price = top_bid.price + 0.04
        else:
            top_bid_price = top_bid.price
            top_ask_price = top_ask.price
            
        delta = self.T / self.N
           
        bid_count = self.bid_count_fn(self.rng)
        bid_Z = self.rng.normal(loc=0, scale=1.0, size=bid_count)
        bid_prices = top_bid_price * np.exp((self.r - 0.5 * self.sigma**2) * delta + self.sigma * np.sqrt(delta) * bid_Z)
        bid_volumes = self.rng.integers(1, 5, size=bid_count)
        
        ask_count = self.ask_count_fn(self.rng)
        ask_Z = self.rng.normal(loc=0, scale=1.0, size=ask_count)
        ask_prices = top_ask_price * np.exp((self.r - 0.5 * self.sigma**2) * delta + self.sigma * np.sqrt(delta) * ask_Z)
        ask_volumes = self.rng.integers(1, 5, size=ask_count)
        
        for j in range(bid_count):
            self.clob.submit_bid(LimitOrder(
                price=round(bid_prices[j], 2), 
                timestamp=self._timestamp, 
                quantity=bid_volumes[j], 
                id=self._action_count
            ))
            self._action_count += 1
            
        for j in range(ask_count):
            self.clob.submit_ask(LimitOrder(
                price=round(ask_prices[j], 2), 
                timestamp=self._timestamp, 
                quantity=ask_volumes[j], 
                id=self._action_count
            ))
            self._action_count += 1
        
        transactions = self.clob.process_transactions()
        depths = self.clob.get_cumulative_depth()
        
        return SimulationStep(
            transactions=transactions,
            depths=depths,
            top_bid=top_bid_price,
            top_ask=top_ask_price
        )
    pass

if __name__ == "__main__":
    pass
