import numpy as np
import numpy.typing as npt

class LowHighDemoSDE():
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

class MeanRevertingSDE():
    _rng: np.random.Generator
    
    def __init__(self, rng: np.random.Generator):
        self._rng = rng
        pass
    
    def get_order_count(self, t: float, dt: float) -> int:
        return 65
    
    def _adjustment_factor(self, t: float, dt: float) -> float:
        return np.sqrt(self.get_order_count(t, dt))
    
    def _unadjusted_volatility(self, t: float) -> float:
        return 0.25
    
    def get_volatility(self, t: float, dt: float) -> float:
        return self._unadjusted_volatility(t) * self._adjustment_factor(t, dt)
    
    def _unadjusted_reversion(self, t: float) -> float:
        return 50 * self._unadjusted_volatility(t)
    
    def _get_reversion(self, t: float, dt: float) -> float:
        return self._unadjusted_reversion(t) * self._adjustment_factor(t, dt)
    
    def get_spread(self, t: float, dt: float) -> float:
        return 0.04
    
    def get_stock_price(self, t: float, dt: float) -> float:
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

