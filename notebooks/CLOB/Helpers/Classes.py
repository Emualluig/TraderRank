from dataclasses import dataclass

@dataclass
class LimitOrder:
    price: float
    timestamp: float
    quantity: float
    id: int
    
    def __repr__(self):
        return f"Order(id={self.id}, price={self.price}, quantity={self.quantity}, ts={self.timestamp})"
    pass

@dataclass
class Transaction:
    price: float
    quantity: float
    pass

@dataclass
class MinMax:
    min: float
    max: float
    
    def mean(self) -> float:
        return (self.min + self.max) / 2.0
    def average(self) -> float:
        return self.mean()
    pass
