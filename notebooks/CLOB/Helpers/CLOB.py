
from typing import List, Tuple
from sortedcontainers import SortedDict
from Helpers.Classes import LimitOrder, Transaction

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
