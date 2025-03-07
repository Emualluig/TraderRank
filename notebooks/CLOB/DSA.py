from dataclasses import dataclass
from typing import Callable, List, Tuple
import numpy as np
import numpy.typing as npt
from sortedcontainers import SortedDict

def BlackScholesFinalPrices(
    S_0: float, 
    r: float, 
    sigma: float, 
    T: float, 
    n: int,
    rng: np.random.Generator
) -> npt.NDArray[np.float64]:
    Z = rng.normal(loc=0, scale=1, size=(n))
    return S_0 * np.exp((r - 0.5 * sigma**2) * T + sigma * np.sqrt(T) * Z)

@dataclass
class LimitOrder:
    price: float
    timestamp: float
    quantity: float
    id: int
    
    def __repr__(self):
        return f"Order(id={self.id}, price={self.price}, quantity={self.quantity}, ts={self.timestamp})"

@dataclass
class Transaction:
    price: float
    quantity: float

class CLOB:
    _bid_queue: SortedDict
    _ask_queue: SortedDict
    _bid_dict: dict[int, Tuple[float, float]]
    _ask_dict: dict[int, Tuple[float, float]]
    
    def _key(self, order: LimitOrder):
        return (order.price, order.timestamp, order.id)
    
    def __init__(self):
        self._bid_queue = SortedDict(lambda k: (-k[0], k[1], k[2]))
        self._ask_queue = SortedDict(lambda k: (k[0], k[1], k[2]))
        self._bid_dict = {}
        self._ask_dict = {}
        
    def submit_bid(self, order: LimitOrder):
        key = self._key(order)
        self._bid_queue[key] = order
        self._bid_dict[order.id] = key
    
    def submit_ask(self, order: LimitOrder):
        key = self._key(order)
        self._ask_queue[key] = order
        self._ask_dict[order.id] = key
        
    def get_all_bid_ids(self):
        return [x for x in self._bid_dict.keys()]    
    
    def get_all_ask_ids(self):
        return [x for x in self._ask_dict.keys()]
        
    def cancel_bid(self, id: int):
        key = self._bid_dict.pop(id)
        self._bid_queue.pop(key)
    
    def cancel_ask(self, id: int):
        key = self._ask_dict.pop(id)
        self._ask_queue.pop(key)
        
    def _remove_top_bid(self):
        kv = self._bid_queue.popitem(0)
        order = kv[1]
        self._bid_dict.pop(order.id)
        
    def _remove_top_ask(self):
        kv = self._ask_queue.popitem(0)
        order = kv[1]
        self._ask_dict.pop(order.id)
        
    def top_bid(self) -> LimitOrder|None:
        length = len(self._bid_queue)
        if length == 0:
            return None
        return self._bid_queue.peekitem(0)[1]
    
    def top_ask(self) -> LimitOrder|None:
        length = len(self._ask_queue)
        if length == 0:
            return None
        return self._ask_queue.peekitem(0)[1]
        
    def process_transactions(self):
        transactions: List[Transaction] = []
        while True:
            bid = self.top_bid()
            ask = self.top_ask()
            if bid != None and ask != None:
                if bid.price >= ask.price:
                    transacted_price = ask.price
                    transacted_quantity = min(bid.quantity, ask.quantity)
                    resulting_bid_quantity = bid.quantity - transacted_quantity
                    resulting_ask_quantity = ask.quantity - transacted_quantity
                    if resulting_bid_quantity == 0:
                        self._remove_top_bid()
                    else:
                        bid.quantity = resulting_bid_quantity
                    
                    if resulting_ask_quantity == 0:
                        self._remove_top_ask()
                    else:
                        ask.quantity = resulting_ask_quantity
                    
                    transactions.append(Transaction(price=transacted_price, quantity=transacted_quantity))
                else:
                    break
            else:
                break
        return transactions
    
    def get_cumulative_depth(self):
        bid_depth: dict[float, float] = {}
        ask_depth: dict[float, float] = {}
        
        cumulative_bid_depth = 0
        for n in self._bid_queue.values():
            order: LimitOrder = n
            cumulative_bid_depth += order.quantity
            bid_depth[order.price] = cumulative_bid_depth
            
        cumulative_ask_depth = 0
        for n in self._ask_queue.values():
            order: LimitOrder = n
            cumulative_ask_depth += order.quantity
            ask_depth[order.price] = cumulative_ask_depth
        
        return bid_depth, ask_depth

class SimulationConst:
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
    
    @dataclass
    class SimulationStep:
        transactions: List[Transaction]
        depths: tuple[dict[float, float], dict[float, float]]
        top_bid: float
        top_ask: float
        pass
    
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
        
        market_mid_price = (top_bid_price + top_ask_price) / 2.0
        Z = self.rng.normal(loc=0, scale=1.0)
        target_price: float = market_mid_price * np.exp((self.r - 0.5 * self.sigma**2) * delta + self.sigma * np.sqrt(delta) * Z)
                    
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
        
        return SimulationConst.SimulationStep(
            transactions=transactions,
            depths=depths,
            top_bid=top_bid_price,
            top_ask=top_ask_price
        )
    pass

if __name__ == "__main__":
    rng = np.random.default_rng(42)
    N=252
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
    )

    for _ in range(N):
        simulation.do_simulation_step()
    pass
