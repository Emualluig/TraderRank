from typing import Callable, List, Tuple
import numpy as np
from Helpers.Classes import LimitOrder, Transaction
from Helpers.CLOB import CLOB
from Helpers.SimulationAbstract import Simulation, SimulationStep

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

    def get_N(self) -> int:
        return self.N

    def _compute_volatility(prices: List[float], window: int):
        if len(prices) < window:
            return np.nan
        return np.std(prices[-window:])
    
    def do_simulation_step(self) -> SimulationStep:
        if self._timestamp > self.N:
            raise Exception("Simulation is finished")
        
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
        
        self._timestamp += 1
        return SimulationStep(
            transactions=transactions,
            depths=depths,
            top_bid=top_bid_price,
            top_ask=top_ask_price
        )
    pass


class SimulationDeepBook(Simulation):
    clob: CLOB
    r: float
    sigma: float
    T: float
    N: float
    removal_percentage: float
    initial_price: float
    initial_spread: float
    rng: np.random.Generator
    bid_count_fn: Callable[[], int]
    ask_count_fn: Callable[[], int]
    _action_count: int
    _timestamp: int
    
    def _generate_book(self):
        bid_average_count = sum(self.bid_count_fn(self.rng) for _ in range(100)) // 100
        ask_average_count = sum(self.ask_count_fn(self.rng) for _ in range(100)) // 100
        count = int((bid_average_count + ask_average_count) / self.removal_percentage) // 2
        price_distribution  = self.rng.uniform(0.75, 1.0, size=count)
        volume_distribution = self.rng.integers(1, 5, size=count)
        sigma_frac = self.sigma / 2.0
        
        bid_top_price = round(self.initial_price - self.initial_spread, 2)
        bid_bottom_price = round(bid_top_price - sigma_frac * bid_top_price, 2)
        bid_prices: List[float] = list(bid_top_price * price_distribution + bid_bottom_price * (1.0 - price_distribution))
        
        ask_bottom_price = round(self.initial_price + self.initial_spread, 2)
        ask_top_price = round(ask_bottom_price + sigma_frac * ask_bottom_price, 2)
        ask_prices: List[float] = list(ask_bottom_price * price_distribution + ask_top_price * (1.0 - price_distribution))
        
        for i, bid_price in enumerate(bid_prices):
            self.clob.submit_bid(LimitOrder(price=bid_price, timestamp=0, quantity=volume_distribution[i], id=self._action_count))
            self._action_count += 1
        
        for i, ask_price in enumerate(ask_prices):
            self.clob.submit_ask(LimitOrder(price=ask_price, timestamp=0, quantity=volume_distribution[i], id=self._action_count))
            self._action_count += 1
        pass
    
    def __init__(self, 
                 r: float, 
                 sigma: float, 
                 T: float, 
                 N: int, 
                 removal_percentage: float,
                 initial_price: float,
                 initial_spread: float,
                 bid_count_fn: Callable[[np.random.Generator], int],
                 ask_count_fn: Callable[[np.random.Generator], int],
                 rng: np.random.Generator,
                ):
        self.clob = CLOB()
        self.r = r
        self.sigma = sigma
        self.T = T
        self.N = N
        self.removal_percentage = removal_percentage
        self.initial_price = initial_price
        self.initial_spread = initial_spread
        self.rng = rng
        self.bid_count_fn = bid_count_fn
        self.ask_count_fn = ask_count_fn
        self._action_count = 0
        self._timestamp = 0
        pass
    
    def get_N(self) -> int:
        return self.N
    
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
        
        first_timestep = False
        if top_bid == None and top_ask == None:
            self._generate_book()
            first_timestep = True
            top_bid_price = self.clob.top_bid().price
            top_ask_price = self.clob.top_ask().price
        elif top_bid == None and top_ask != None:
            top_bid_price = top_ask.price - self.initial_spread
            top_ask_price = top_ask.price
        elif top_bid != None and top_ask == None:
            top_bid_price = top_bid.price
            top_ask_price = top_bid.price + self.initial_spread
        else:
            top_bid_price = top_bid.price
            top_ask_price = top_ask.price
            
        if not first_timestep:
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
            pass
        
        transactions = self.clob.process_transactions()
        depths = self.clob.get_cumulative_depth()
        
        return SimulationStep(
            transactions=transactions,
            depths=depths,
            top_bid=top_bid_price,
            top_ask=top_ask_price
        )
    pass

class OrderSDE_V1():
    _rng: np.random.Generator
    
    def __init__(self, rng: np.random.Generator):
        self._rng = rng
        pass
    
    def get_order_count(self, t: float, dt: float) -> int:
        return int(15 * self._unadjusted_volatility(t) + 5)
    
    def _adjustment_factor(self, order_count: int) -> float:
        return np.sqrt(order_count)
    
    def _unadjusted_volatility(self, t: float) -> float:
        if 0.8 <= t:
            return 0.025
        elif 0.4 <= t <= 0.6:
            return 1.0
        else:
            return 0.2
    
    def get_volatility(self, t: float, dt: float) -> float:
        return 3.0 * self._adjustment_factor(self.get_order_count(t, dt)) * self._unadjusted_volatility(t)
    
    def _unadjusted_reversion(self, t: float) -> float:
        if 0.8 <= t:
            return 30.0
        elif 0.5 <= t:
            return self._unadjusted_volatility(t) * (5.0 + 30 * t)
        else:
            return self._unadjusted_volatility(t) * 5.0
    
    def _get_reversion(self, t: float, dt: float) -> float:
        return self._adjustment_factor(self.get_order_count(t, dt)) * self._unadjusted_reversion(t)
    
    def get_spread(self, t: float, dt: float) -> float:
        return 0.04
    
    def get_stock_price(self, t: float, dt: float) -> float:
        if 0.5 <= t:
            return 110.0
        else:
            return 100.0
    
    def generate_bid_price_array(self, price: float, t: float, dt: float) -> npt.NDArray[np.float64]:
        order_count = self.get_order_count(t, dt)
        half_spread = self.get_spread(t, dt) / 2.0
        target = self.get_stock_price(t, dt)
        volatility = self.get_volatility(t, dt)
        reversion = self._get_reversion(t, dt)
        
        return price - half_spread \
               + reversion * (target - price) * dt \
               + volatility * np.sqrt(price * dt) \
               * self._rng.normal(loc=0.0, scale=1.0, size=order_count)
    
    def generate_ask_price_array(self, price: float, t: float, dt: float) -> npt.NDArray[np.float64]:
        order_count = self.get_order_count(t, dt)
        half_spread = self.get_spread(t, dt) / 2.0
        target = self.get_stock_price(t, dt)
        volatility = self.get_volatility(t, dt)
        reversion = self._get_reversion(t, dt)
        
        return price + half_spread \
               + reversion * (target - price) * dt \
               + volatility * np.sqrt(price * dt) \
               * self._rng.normal(loc=0.0, scale=1.0, size=order_count)
    pass


class SimulationOrderSDE_V1(Simulation):
    clob: CLOB
    T: float
    N: int
    order_model: OrderSDE_V1
    removal_percentage: float
    _rng: np.random.Generator
    _action_count: int
    _timestamp: int
    
    def __init__(self, 
                 T: float, 
                 N: int, 
                 order_model: OrderSDE_V1,
                 removal_percentage: float,
                 rng: np.random.Generator,
                ):
        self.clob = CLOB()
        self.T = T
        self.N = N
        self.order_model = order_model
        self.removal_percentage = removal_percentage
        self._rng = rng
        self._action_count = 0
        self._timestamp = 0
        pass
    
    def get_N(self) -> int:
        return self.N
    
    def _remove_orders(self, percent: float):
        # Remove some percent of random orders
        bid_ids = self.clob.get_all_bid_ids()        
        if (k := int(len(bid_ids) * percent)) > 0:
            bids_to_remove = list(self._rng.choice(a=bid_ids, size=k, replace=False))
            for bid_id in bids_to_remove:
                self.clob.cancel_bid(bid_id)        
            
        ask_ids = self.clob.get_all_ask_ids()
        if (k := int(len(ask_ids) * percent)) > 0:            
            asks_to_remove = list(self._rng.choice(a=ask_ids, size=k, replace=False))
            for ask_id in asks_to_remove:
                self.clob.cancel_ask(ask_id)
        pass
    
    def do_simulation_step(self) -> SimulationStep:
        if self._timestamp > self.N:
            raise Exception("Simulation is finished")
        
        dt = self.T / self.N
        t = self._timestamp * dt
        ORDERS_COUNT = self.order_model.get_order_count(t, dt)
        ORDERS_SIZE = MinMax(min=1, max=5)
        
        if t == 0.0:
            # initialize the book
            price_distribution  = self._rng.uniform(0.75, 1.0, size=ORDERS_COUNT)
            volume_distribution = self._rng.integers(ORDERS_SIZE.min, ORDERS_SIZE.max, size=ORDERS_COUNT)
            target = self.order_model.get_stock_price(t, dt)
            volatility = self.order_model.get_volatility(t, dt)
            SPREAD = self.order_model.get_spread(t, dt)
            HALF_SPREAD = SPREAD / 2

            bid_top_price = round(target - HALF_SPREAD, 2)
            bid_bottom_price = round(bid_top_price - 0.5 * volatility * bid_top_price, 2)
            bid_prices: List[float] = list(bid_top_price * price_distribution + bid_bottom_price * (1.0 - price_distribution))
            
            ask_bottom_price = round(target + HALF_SPREAD, 2)
            ask_top_price = round(ask_bottom_price + 0.5 * volatility * ask_bottom_price, 2)
            ask_prices: List[float] = list(ask_bottom_price * price_distribution + ask_top_price * (1.0 - price_distribution))
            
            for i, bid_price in enumerate(bid_prices):
                self.clob.submit_bid(LimitOrder(price=bid_price, timestamp=0, quantity=volume_distribution[i], id=self._action_count))
                self._action_count += 1
            
            for i, ask_price in enumerate(ask_prices):
                self.clob.submit_ask(LimitOrder(price=ask_price, timestamp=0, quantity=volume_distribution[i], id=self._action_count))
                self._action_count += 1
            pass
        else:  
            self._remove_orders(self.removal_percentage)
            
            top_bid_price = self.clob.top_bid().price
            top_ask_price = self.clob.top_ask().price
            
            bid_prices = self.order_model.generate_bid_price_array(top_bid_price, t, dt)
            ask_prices = self.order_model.generate_ask_price_array(top_ask_price, t, dt)
            
            bid_quantities = self._rng.integers(ORDERS_SIZE.min, ORDERS_SIZE.max, size=ORDERS_COUNT) 
            for i, bid_price in enumerate(bid_prices):
                self.clob.submit_bid(LimitOrder(price=round(bid_price, 2), timestamp=self._timestamp, quantity=bid_quantities[i], id=self._action_count))
                self._action_count += 1
                pass
            
            ask_quantities = self._rng.integers(ORDERS_SIZE.min, ORDERS_SIZE.max, size=ORDERS_COUNT) 
            for i, ask_price in enumerate(ask_prices):
                self.clob.submit_ask(LimitOrder(price=round(ask_price, 2), timestamp=self._timestamp, quantity=ask_quantities[i], id=self._action_count))
                self._action_count += 1
                pass
            pass
        
        transactions = self.clob.process_transactions()
        depths = self.clob.get_cumulative_depth()
        
        top_bid_price = self.clob.top_bid().price
        top_ask_price = self.clob.top_ask().price
        
        self._timestamp += 1
        return SimulationStep(
            transactions=transactions,
            depths=depths,
            top_bid=top_bid_price,
            top_ask=top_ask_price
        )
    pass

if __name__ == "__main__":
    pass
