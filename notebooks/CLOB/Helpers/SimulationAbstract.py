from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import List
from Helpers.Classes import Transaction


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
    
    @abstractmethod
    def get_N(self) -> int: ...
    pass
