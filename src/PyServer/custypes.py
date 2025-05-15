from dataclasses import dataclass
import dataclasses
from enum import Enum
import json
from typing import List, Union

class EnhancedJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        if isinstance(o, Enum):
            return o.value
        return super().default(o)

@dataclass
class LimitOrder:
    order_id: int
    price: float
    user_id: int
    volume: float

@dataclass
class OrderBook:
    bids: List[LimitOrder]
    asks: List[LimitOrder]

@dataclass
class Transaction:
    tick: int
    price: float
    volume: float
    seller_id: int
    buyer_id: int

@dataclass
class SecurityInfo:
    security_id: int
    decimal_places: int
    net_limit: float
    gross_limit: float
    max_trade_volume: float

@dataclass
class News:
    tick: int
    text: str

class SimulationState(Enum):
    running = "running"
    paused = "paused"

class MessageType(Enum):
    login_request = "login_request"
    login_response = "login_response"
    simulation_load = "simulation_load"
    simulation_update = "simulation_update"
    market_update = "market_update"
    new_user_connected = "new_user_connected"
    chat_message_received = "chat_message_received"

@dataclass
class MessageBase:
    pass

@dataclass
class MessageLoginRequest(MessageBase):
    username: str
    type_: MessageType = MessageType.login_request

@dataclass
class MessageLoginResponse(MessageBase):
    user_id: int
    type_: MessageType = MessageType.login_response

@dataclass
class MessageSimulationLoad(MessageBase):
    simulation_state: SimulationState
    tick: int
    max_tick: int
    all_securities: List[str]
    tradeable_securities: List[str]
    security_info: dict[str, SecurityInfo]
    order_book_per_security: dict[str, OrderBook]
    transactions: dict[str, List[Transaction]]
    user_id_to_username: dict[int, str]
    portfolio: dict[str, float]
    news: List[News]
    type_: MessageType = MessageType.simulation_load

@dataclass
class MessageSimulationUpdate(MessageBase):
    simulation_state: SimulationState
    tick: int
    type_: MessageType = MessageType.simulation_update

@dataclass
class MessageMarketUpdate(MessageBase):
    tick: int
    order_book_per_security: dict[str, OrderBook]
    portfolio: dict[str, float]
    new_transactions: dict[str, List[Transaction]]
    new_news: List[News]
    type_: MessageType = MessageType.market_update

@dataclass
class MessageNewUserConnected(MessageBase):
    user_id: int
    username: str
    type_: MessageType = MessageType.new_user_connected

@dataclass
class MessageChatMessageReceived(MessageBase):
    user_id: int
    text: str
    type_: MessageType = MessageType.chat_message_received

type Message = Union[
    MessageLoginRequest,
    MessageLoginResponse,
    MessageSimulationLoad,
    MessageSimulationUpdate,
    MessageMarketUpdate,
    MessageNewUserConnected,
    MessageChatMessageReceived
]