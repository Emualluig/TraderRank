from __future__ import annotations
import typing
__all__ = ['ASK', 'BID', 'CancelOrder', 'GenericSimulation', 'IPortfolioManager', 'ISecurity', 'ISimulation', 'LimitOrder', 'LimitOrderList', 'OrderSide', 'PriceDepthMap', 'SimulationStepResult', 'Transaction']
class CancelOrder:
    def __init__(self) -> None:
        ...
    @property
    def order_id(self) -> int:
        ...
    @order_id.setter
    def order_id(self, arg0: typing.SupportsInt) -> None:
        ...
    @property
    def user_id(self) -> int:
        ...
    @user_id.setter
    def user_id(self, arg0: typing.SupportsInt) -> None:
        ...
class GenericSimulation(ISimulation):
    def __init__(self, arg0: dict[str, ISecurity], arg1: typing.SupportsFloat, arg2: typing.SupportsInt) -> None:
        ...
class IPortfolioManager:
    def __init__(self) -> None:
        ...
    def add_to_security(self, user_id: typing.SupportsInt, security_id: typing.SupportsInt, addition: typing.SupportsFloat) -> float:
        ...
    def add_to_two_securities(self, user_id: typing.SupportsInt, security_1: typing.SupportsInt, addition_1: typing.SupportsFloat, security_2: typing.SupportsInt, addition_2: typing.SupportsFloat) -> tuple[float, float]:
        ...
    def get_portfolio_table(self) -> list[list[float]]:
        ...
    def multiply_and_add_1_to_2(self, user_id: typing.SupportsInt, security_1: typing.SupportsInt, security_2: typing.SupportsInt, multiply: typing.SupportsFloat) -> float:
        ...
    def multiply_and_add_1_to_2_and_set_1(self, user_id: typing.SupportsInt, security_1: typing.SupportsInt, security_2: typing.SupportsInt, multiply: typing.SupportsFloat, set_value: typing.SupportsFloat) -> float:
        ...
    def reset_user_portfolio(self, user_id: typing.SupportsInt) -> None:
        ...
class ISecurity:
    def __init__(self) -> None:
        ...
    def after_step(self, simulation: ISimulation, portfolio: IPortfolioManager) -> None:
        ...
    def before_step(self, simulation: ISimulation, portfolio: IPortfolioManager) -> None:
        ...
    def is_tradeable(self) -> bool:
        ...
    def on_simulation_end(self, simulation: ISimulation, portfolio: IPortfolioManager) -> None:
        ...
    def on_simulation_start(self, simulation: ISimulation, portfolio: IPortfolioManager) -> None:
        ...
    def on_trade_executed(self, simulation: ISimulation, portfolio: IPortfolioManager, buyer_id: typing.SupportsInt, seller_id: typing.SupportsInt, transacted_price: typing.SupportsFloat, transacted_volume: typing.SupportsFloat) -> None:
        ...
class ISimulation:
    def __init__(self, securities: dict[str, ISecurity], T: typing.SupportsFloat, N: typing.SupportsInt) -> None:
        ...
    def add_user(self, username: str) -> int:
        ...
    def do_simulation_step(self) -> SimulationStepResult:
        ...
    def get_N(self) -> int:
        ...
    def get_T(self) -> float:
        ...
    def get_all_open_user_orders(self, user_id: typing.SupportsInt, security_id: typing.SupportsInt) -> set[int]:
        ...
    def get_all_tickers(self) -> list[str]:
        ...
    def get_ask_count(self, security_id: typing.SupportsInt) -> int:
        ...
    def get_bid_count(self, security_id: typing.SupportsInt) -> int:
        ...
    def get_cumulative_book_depth(self, security_id: typing.SupportsInt) -> tuple[dict[float, float], dict[float, float]]:
        ...
    def get_dt(self) -> float:
        ...
    def get_order_book(self, security_id: typing.SupportsInt) -> tuple[list[LimitOrder], list[LimitOrder]]:
        ...
    def get_securities_count(self) -> int:
        ...
    def get_security_id(self, security_ticker: str) -> int:
        ...
    def get_security_ticker(self, security_id: typing.SupportsInt) -> str:
        ...
    def get_t(self) -> float:
        ...
    def get_tick(self) -> int:
        ...
    def get_top_ask(self, security_id: typing.SupportsInt) -> LimitOrder:
        ...
    def get_top_bid(self, security_id: typing.SupportsInt) -> LimitOrder:
        ...
    def get_user_count(self) -> int:
        ...
    def get_user_id_to_username(self) -> dict[int, str]:
        ...
    def reset_simulation(self) -> None:
        ...
    def submit_cancel_order(self, user_id: typing.SupportsInt, security_id: typing.SupportsInt, order_id: typing.SupportsInt) -> None:
        ...
    def submit_limit_order(self, user_id: typing.SupportsInt, security_id: typing.SupportsInt, side: OrderSide, price: typing.SupportsFloat, volume: typing.SupportsFloat) -> int:
        ...
class LimitOrder:
    side: OrderSide
    def __init__(self) -> None:
        ...
    @property
    def order_id(self) -> int:
        ...
    @order_id.setter
    def order_id(self, arg0: typing.SupportsInt) -> None:
        ...
    @property
    def price(self) -> float:
        ...
    @price.setter
    def price(self, arg0: typing.SupportsFloat) -> None:
        ...
    @property
    def user_id(self) -> int:
        ...
    @user_id.setter
    def user_id(self, arg0: typing.SupportsInt) -> None:
        ...
    @property
    def volume(self) -> float:
        ...
    @volume.setter
    def volume(self, arg0: typing.SupportsFloat) -> None:
        ...
class LimitOrderList:
    def __bool__(self: list[LimitOrder]) -> bool:
        """
        Check whether the list is nonempty
        """
    @typing.overload
    def __delitem__(self: list[LimitOrder], arg0: typing.SupportsInt) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self: list[LimitOrder], arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    @typing.overload
    def __getitem__(self: list[LimitOrder], s: slice) -> list[LimitOrder]:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self: list[LimitOrder], arg0: typing.SupportsInt) -> LimitOrder:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: list[LimitOrder]) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self: list[LimitOrder]) -> typing.Iterator[LimitOrder]:
        ...
    def __len__(self: list[LimitOrder]) -> int:
        ...
    @typing.overload
    def __setitem__(self: list[LimitOrder], arg0: typing.SupportsInt, arg1: LimitOrder) -> None:
        ...
    @typing.overload
    def __setitem__(self: list[LimitOrder], arg0: slice, arg1: list[LimitOrder]) -> None:
        """
        Assign list elements using a slice object
        """
    def append(self: list[LimitOrder], x: LimitOrder) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self: list[LimitOrder]) -> None:
        """
        Clear the contents
        """
    @typing.overload
    def extend(self: list[LimitOrder], L: list[LimitOrder]) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self: list[LimitOrder], L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self: list[LimitOrder], i: typing.SupportsInt, x: LimitOrder) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self: list[LimitOrder]) -> LimitOrder:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self: list[LimitOrder], i: typing.SupportsInt) -> LimitOrder:
        """
        Remove and return the item at index ``i``
        """
class OrderSide:
    """
    Members:
    
      BID
    
      ASK
    """
    ASK: typing.ClassVar[OrderSide]  # value = <OrderSide.ASK: 1>
    BID: typing.ClassVar[OrderSide]  # value = <OrderSide.BID: 0>
    __members__: typing.ClassVar[dict[str, OrderSide]]  # value = {'BID': <OrderSide.BID: 0>, 'ASK': <OrderSide.ASK: 1>}
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt) -> None:
        ...
    def __int__(self) -> int:
        ...
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class PriceDepthMap:
    def __bool__(self: dict[typing.SupportsFloat, typing.SupportsFloat]) -> bool:
        """
        Check whether the map is nonempty
        """
    @typing.overload
    def __contains__(self: dict[typing.SupportsFloat, typing.SupportsFloat], arg0: typing.SupportsFloat) -> bool:
        ...
    @typing.overload
    def __contains__(self: dict[typing.SupportsFloat, typing.SupportsFloat], arg0: typing.Any) -> bool:
        ...
    def __delitem__(self: dict[typing.SupportsFloat, typing.SupportsFloat], arg0: typing.SupportsFloat) -> None:
        ...
    def __getitem__(self: dict[typing.SupportsFloat, typing.SupportsFloat], arg0: typing.SupportsFloat) -> float:
        ...
    def __init__(self) -> None:
        ...
    def __iter__(self: dict[typing.SupportsFloat, typing.SupportsFloat]) -> typing.Iterator[float]:
        ...
    def __len__(self: dict[typing.SupportsFloat, typing.SupportsFloat]) -> int:
        ...
    def __repr__(self: dict[typing.SupportsFloat, typing.SupportsFloat]) -> str:
        """
        Return the canonical string representation of this map.
        """
    def __setitem__(self: dict[typing.SupportsFloat, typing.SupportsFloat], arg0: typing.SupportsFloat, arg1: typing.SupportsFloat) -> None:
        ...
    def items(self: dict[typing.SupportsFloat, typing.SupportsFloat]) -> typing.ItemsView:
        ...
    def keys(self: dict[typing.SupportsFloat, typing.SupportsFloat]) -> typing.KeysView:
        ...
    def values(self: dict[typing.SupportsFloat, typing.SupportsFloat]) -> typing.ValuesView:
        ...
class SimulationStepResult:
    has_next_step: bool
    order_book_per_security: dict[str, tuple[list[LimitOrder], list[LimitOrder]]]
    transactions: dict[str, list[Transaction]]
    @property
    def cancelled_orders(self) -> dict[str, set[int]]:
        ...
    @cancelled_orders.setter
    def cancelled_orders(self, arg0: dict[str, set[typing.SupportsInt]]) -> None:
        ...
    @property
    def current_step(self) -> int:
        ...
    @current_step.setter
    def current_step(self, arg0: typing.SupportsInt) -> None:
        ...
    @property
    def fully_transacted_orders(self) -> dict[str, set[int]]:
        ...
    @fully_transacted_orders.setter
    def fully_transacted_orders(self, arg0: dict[str, set[typing.SupportsInt]]) -> None:
        ...
    @property
    def order_book_depth_per_security(self) -> dict[str, tuple[dict[float, float], dict[float, float]]]:
        ...
    @order_book_depth_per_security.setter
    def order_book_depth_per_security(self, arg0: dict[str, tuple[dict[typing.SupportsFloat, typing.SupportsFloat], dict[typing.SupportsFloat, typing.SupportsFloat]]]) -> None:
        ...
    @property
    def partially_transacted_orders(self) -> dict[str, dict[int, float]]:
        ...
    @partially_transacted_orders.setter
    def partially_transacted_orders(self, arg0: dict[str, dict[typing.SupportsInt, typing.SupportsFloat]]) -> None:
        ...
    @property
    def portfolios(self) -> list[list[float]]:
        ...
    @portfolios.setter
    def portfolios(self, arg0: list[list[typing.SupportsFloat]]) -> None:
        ...
    @property
    def user_id_to_username_map(self) -> dict[int, str]:
        ...
    @user_id_to_username_map.setter
    def user_id_to_username_map(self, arg0: dict[typing.SupportsInt, str]) -> None:
        ...
class Transaction:
    @property
    def buyer_id(self) -> int:
        ...
    @buyer_id.setter
    def buyer_id(self, arg0: typing.SupportsInt) -> None:
        ...
    @property
    def price(self) -> float:
        ...
    @price.setter
    def price(self, arg0: typing.SupportsFloat) -> None:
        ...
    @property
    def seller_id(self) -> int:
        ...
    @seller_id.setter
    def seller_id(self, arg0: typing.SupportsInt) -> None:
        ...
    @property
    def volume(self) -> float:
        ...
    @volume.setter
    def volume(self, arg0: typing.SupportsFloat) -> None:
        ...
ASK: OrderSide  # value = <OrderSide.ASK: 1>
BID: OrderSide  # value = <OrderSide.BID: 0>
