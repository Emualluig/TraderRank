"""
Generic security types
"""
import Server
from __future__ import annotations
import typing
__all__ = ['DividendStock', 'GenericBond', 'GenericCurrency', 'GenericStock', 'MarginCash']
class DividendStock(Server.ISecurity):
    def __init__(self, ticker: str, currency: str, dividend_function: typing.Callable[[typing.SupportsInt], float]) -> None:
        ...
class GenericBond(Server.ISecurity):
    def __init__(self, ticker: str, currency: str, rate: typing.SupportsFloat, face_value: typing.SupportsFloat) -> None:
        ...
class GenericCurrency(Server.ISecurity):
    def __init__(self, ticker: str) -> None:
        ...
class GenericStock(Server.ISecurity):
    def __init__(self, ticker: str, currency: str) -> None:
        ...
class MarginCash(Server.ISecurity):
    def __init__(self, ticker: str, margin_interest_rate: typing.SupportsFloat, starting_cash: typing.SupportsFloat) -> None:
        ...
