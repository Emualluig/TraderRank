"""
Generic security types
"""
import Server
from __future__ import annotations
import typing
__all__ = ['GenericBond', 'GenericCurrency', 'GenericStock']
class GenericBond(Server.ISecurity):
    def __init__(self, ticker: str, currency: str, rate: typing.SupportsFloat, face_value: typing.SupportsFloat) -> None:
        ...
class GenericCurrency(Server.ISecurity):
    def __init__(self, ticker: str) -> None:
        ...
class GenericStock(Server.ISecurity):
    def __init__(self, ticker: str, currency: str) -> None:
        ...
