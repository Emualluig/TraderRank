import asyncio
from dataclasses import dataclass
import json
from typing import List, Tuple
import websockets
import python_modules.Server as Server
from custypes import CancelledOrders, EnhancedJSONEncoder, LimitOrder, MessageLoginResponse, MessageMarketUpdate, MessageSimulationLoad, MessageSimulationUpdate, OrderBook, SecurityInfo, SimulationState, SubmittedOrders, TransactedOrders, Transaction
import numpy as np

def limit_order_convert(order: Server.LimitOrder) -> LimitOrder:
    return LimitOrder(
        order_id=order.order_id, 
        price=order.price,
        volume=order.volume,
        user_id=order.user_id
    )

def volatility_strength_func(tick: int, t: float, dt: float):
    if 0 <= tick < 200:
        return 0.5
    elif 200 <= tick < 400:
        return 1.0
    elif 400 <= tick < 500:
        return 2.5
    elif 500 <= tick < 800:
        return 1.0
    elif 800 <= tick < 900:
        return 2.5
    else:
        return 0.5

def reversion_strength_func(tick: int, t: float, dt: float):
    return 100

def leaky_reversion_strength_func(tick: int, t: float, dt: float):
    return 10

def get_base_path(
    initial_price: float,
    up_price: float,
    down_price: float,
    total_ticks: int,
    extra_ticks: int,
    preliminary_probability: float,
    fda_probability: float,
    rng: np.random.Generator,
):
    had_good_preliminary_results = rng.uniform(0, 1.0) < preliminary_probability
    had_fda_accepted = rng.uniform(0, 1.0) < fda_probability if had_good_preliminary_results else rng.uniform(0, 1.0) > fda_probability
    base_bath = initial_price * np.ones(shape=(total_ticks + extra_ticks))
    base_bath[500:900] = (
        fda_probability * up_price + (1-fda_probability) * down_price 
            if had_good_preliminary_results else 
        (1-fda_probability) * up_price + fda_probability * down_price
    )
    base_bath[900:] = up_price if had_fda_accepted else down_price
    return base_bath, had_good_preliminary_results, had_fda_accepted

positive_preliminary_blurbs = [
    "BIOTECH announces promising preliminary Phase III trial results for its flagship drug Xeronex. Early data suggests significant efficacy improvements over existing treatments, with a strong safety profile. The company is preparing its FDA submission.",
    "BIOTECH reports early success in Xeronex trials. Patients in the trial group exhibited a marked improvement over placebo, with minimal adverse effects noted.",
    "Xeronex shows early promise: BIOTECH's lead candidate surpassed expectations in efficacy metrics. Investors hopeful for FDA green light.",
    "Strong preliminary data boosts BIOTECH outlook. Internal sources say response rates “far exceeded baseline”, with low dropout rates.",
]
negative_preliminary_blurbs = [
    "BIOTECH releases preliminary results of its Xeronex trial. While some efficacy was observed, the overall results fell short of expectations. Concerns remain about the statistical strength and side effects profile.",
    "Initial trial data for Xeronex underwhelms. While some therapeutic effects observed, results fall short of benchmarks.",
    "BIOTECH's Xeronex stumbles in early findings. Analysts cite “inconclusive efficacy” and “uncertain path forward.”",
    "Concerns mount as Xeronex fails to meet key trial endpoints. Company shares dip as confidence wavers.",
]
positive_fda_blurbs = [
    "The FDA has approved BIOTECH’s new drug Xeronex for market release. Analysts expect a major boost to the company’s revenues as it becomes the first therapy of its kind to reach commercial availability.",
    "FDA gives green light to BIOTECH’s Xeronex. Approval positions company as a front-runner in new therapeutics.",
    "Historic day for BIOTECH: Xeronex approved for use in the U.S. Market analysts expect blockbuster revenue potential.",
    "Regulatory win: FDA endorses Xeronex after thorough review. CEO cites “relentless innovation” and patient-focused development.",
]
negative_fda_blurbs = [
    "The FDA has rejected BIOTECH’s application for Xeronex. The agency cited concerns over insufficient efficacy and unresolved safety issues in the final submission package.",
    "FDA turns down Xeronex application, citing data inconsistencies and safety concerns. BIOTECH expected to revise and resubmit.",
    "BIOTECH setback: FDA rejects Xeronex. Company vows to conduct additional studies and address regulator concerns.",
    "Approval hopes dashed as Xeronex fails to secure FDA clearance. “Disappointing but not surprising,” says one analyst.",
]

@dataclass
class StepResult:
    tick: int
    submitted_orders: dict[str, SubmittedOrders]
    cancelled_orders: dict[str, CancelledOrders]
    transacted_orders: dict[str, TransactedOrders]
    order_book_per_security: dict[str, OrderBook]
    portfolios: List[List[float]]
    pass

class SimulationBiotech:
    def __init__(self):
        # Preliminary result | FDA Decision | Probability | Final Price
        # GOOD (50%) ($125)  | ACCEPT (75%) | 37.5%       | $150
        # GOOD (50%) ($125)  | REJECT (25%) | 12.5%       | $50
        # BAD  (50%) ($75)   | ACCEPT (25%) | 12.5%       | $150
        # BAD  (50%) ($75)   | REJECT (75%) | 37.5%       | $50
        #
        # Start price at $100
        # from step 0 to 200, random drift
        # from step 200 to 400 increased volatility
        # from 400 to 500, increased volatility
        # At 500, get preliminary results
        # from 500 to 800, decreased volatility
        # from 800 to 900, increased volatility
        # At 900, FDA releases decision
        # From 900 to 1000 convergence to true price (either 50 or 150), low volatility
        
        self.rng = np.random.default_rng()
        
        self.total_steps = 1_000
        self.extra_steps = 100
        self.currency = Server.GenericSecurities.GenericCurrency("CAD")
        self.stock = Server.GenericSecurities.GenericStock("BIOTECH", "CAD")
        self.simulation = Server.GenericSimulation(
            { "CAD": self.currency, "BIOTECH": self.stock }, 1.0, self.total_steps
        )
        self.currency_id = self.simulation.get_security_id("CAD")
        self.stock_id = self.simulation.get_security_id("BIOTECH")
        self.anon_id = self.simulation.add_user("AGENT")
        
        self.initial_price = 100.0
        self.up_price = 150.0
        self.down_price = 50.0
        
        self.reset()
        pass
    
    def register_user(self, username: str) -> int:
        return self.simulation.add_user(username)
    
    async def handle_message(self, client_id: str, message: str):
        print(f"[Simulation] Received from {client_id}: {message}")
        pass
    
    def get_tick(self) -> int:
        return self.simulation.get_tick()
    
    def step(self):
        tick = self.simulation.get_tick()
        print(f"On tick: {tick}")
        t = self.simulation.get_t()
        dt = self.simulation.get_dt()
        volatility = volatility_strength_func(tick, t, dt)
        target = self.base_path[tick]
        future_target = self.base_path[tick + self.extra_steps]
        reversion = reversion_strength_func(tick, t, dt)
        leaky_reversion = leaky_reversion_strength_func(tick, t, dt)
        SPREAD = 0.02
        ORDER_SIZE_MIN = 1
        ORDER_SIZE_MAX = 25
        
        if tick == 0:
            assert self.simulation.get_ask_count(self.stock_id) + self.simulation.get_ask_count(self.stock_id) == 0
            
            price_distribution = self.rng.uniform(0.75, 1.0, size=50)
            volume_distribution = self.rng.integers(ORDER_SIZE_MIN, ORDER_SIZE_MAX, 50)

            bid_top_price = target - SPREAD
            bid_bottom_price = bid_top_price - 0.1 * volatility * bid_top_price
            bid_prices: List[float] = list(bid_top_price * price_distribution + bid_bottom_price * (1.0 - price_distribution))
            bids = [(Server.OrderSide.BID, price, volume_distribution[i]) for i, price in enumerate(bid_prices)]
            
            ask_bottom_price = target + SPREAD
            ask_top_price = ask_bottom_price + 0.1 * volatility * ask_bottom_price
            ask_prices: List[float] = list(ask_bottom_price * price_distribution + ask_top_price * (1.0 - price_distribution))
            asks = [(Server.OrderSide.ASK, price, volume_distribution[i]) for i, price in enumerate(ask_prices)]

            combined_orders: List[Tuple[Server.OrderSide, float, float]] = bids + asks
            self.rng.shuffle(combined_orders)

            for (side, price, volume) in combined_orders:
                self.simulation.direct_insert_limit_order(self.anon_id, self.stock_id, side, round(price, 2), volume)
                pass
            pass
        else:
            order_count = 5
            
            orders_ANON = list(self.simulation.get_all_open_user_orders(self.anon_id, self.stock_id))
            # Remove some percent of ANON orders
            if (k := int(len(orders_ANON) * self.removal_percentage)) > 0:
                orders_to_remove = list(self.rng.choice(a=orders_ANON, size=k, replace=False))
                for order_id in orders_to_remove:
                    self.simulation.submit_cancel_order(self.anon_id, self.stock_id, order_id)
                    pass
                pass
            
            if self.simulation.get_bid_count(self.stock_id) > 0 and self.simulation.get_ask_count(self.stock_id) > 0:
                top_bid_price = self.simulation.get_top_bid(self.stock_id).price
                top_ask_price = self.simulation.get_top_ask(self.stock_id).price
            elif self.simulation.get_bid_count(self.stock_id) > 0:
                top_bid_price = self.simulation.get_top_bid(self.stock_id).price
                top_ask_price = top_bid_price + 0.5
            elif self.simulation.get_ask_count(self.stock_id) > 0:
                top_ask_price = self.simulation.get_top_ask(self.stock_id).price
                top_bid_price = top_ask_price + 0.5
            else:
                midpoint = self.stock_midpoints[-1]
                top_bid_price = midpoint + 0.5
                top_ask_price = midpoint + 0.5
            
            bid_prices = (
                top_bid_price - SPREAD
                + reversion * (target - top_bid_price) * dt
                + leaky_reversion * (future_target - top_bid_price) * dt
                + volatility * np.sqrt(top_bid_price * dt)
                * self.rng.normal(loc=0.0, scale=1.0, size=order_count)
            )
            bid_quantities = self.rng.integers(ORDER_SIZE_MIN, ORDER_SIZE_MAX, size=order_count)
            bids = [(Server.OrderSide.BID, price, bid_quantities[i]) for i, price in enumerate(bid_prices)]
            
            ask_prices = (
                top_bid_price + SPREAD
                + reversion * (target - top_ask_price) * dt
                + leaky_reversion * (future_target - top_ask_price) * dt
                + volatility * np.sqrt(top_ask_price * dt)
                * self.rng.normal(loc=0.0, scale=1.0, size=order_count)
            )
            ask_quantities = self.rng.integers(ORDER_SIZE_MIN, ORDER_SIZE_MAX, size=order_count) 
            asks = [(Server.OrderSide.ASK, price, ask_quantities[i]) for i, price in enumerate(ask_prices)]
            
            combined_orders: List[Tuple[Server.OrderSide, float, float]] = bids + asks
            self.rng.shuffle(combined_orders)

            for (side, price, volume) in combined_orders:
                self.simulation.submit_limit_order(self.anon_id, self.stock_id, side, round(price, 2), volume)
                pass
            pass
        
        results = self.simulation.do_simulation_step()
        
        if self.simulation.get_bid_count(self.stock_id) > 0 and self.simulation.get_ask_count(self.stock_id) > 0:
            top_bid_price = self.simulation.get_top_bid(self.stock_id).price
            top_ask_price = self.simulation.get_top_ask(self.stock_id).price
        elif self.simulation.get_bid_count(self.stock_id) > 0:
            top_bid_price = self.simulation.get_top_bid(self.stock_id).price
            top_ask_price = top_bid_price + 0.5
        elif self.simulation.get_ask_count(self.stock_id) > 0:
            top_ask_price = self.simulation.get_top_ask(self.stock_id).price
            top_bid_price = top_ask_price + 0.5
        else:
            top_bid_price = self.stock_midpoints[-1] - 0.5
            top_ask_price = self.stock_midpoints[-1] + 0.5
        self.stock_midpoints.append((top_bid_price + top_ask_price) / 2.0)
        
        done = not results.has_next_step
        
        w2: dict[str, SubmittedOrders] = {ticker: SubmittedOrders(bid=[], ask=[]) for ticker in results.v2_submitted_orders.keys()}
        for (ticker, ob) in results.v2_submitted_orders.items():
            for lo in ob:
                if lo.side == Server.OrderSide.BID:
                    w2[ticker].bid.append(LimitOrder(order_id=lo.order_id, price=lo.price, user_id=lo.user_id, volume=lo.volume))
                    pass
                elif lo.side == Server.OrderSide.ASK:
                    w2[ticker].ask.append(LimitOrder(order_id=lo.order_id, price=lo.price, user_id=lo.user_id, volume=lo.volume))
                    pass
                pass
            pass
        
        to = {ticker: 
            list(pair.items())
         for (ticker, pair) in results.v2_transacted_orders.items()}
        
        ob = [
            (ticker, current_case.simulation.get_order_book(current_case.simulation.get_security_id(ticker)))
            for ticker in current_case.simulation.get_all_tickers()
        ]
        kob = {ticker: OrderBook(
            bid=[limit_order_convert(l) for l in book[0]],
            ask=[limit_order_convert(l) for l in book[1]] 
        ) for ticker, book in ob}
        
        return done, StepResult(
            tick=self.simulation.get_tick(),
            submitted_orders=w2,
            cancelled_orders=results.v2_cancelled_orders,
            transacted_orders=to,
            order_book_per_security=kob,
            portfolios=results.portfolios,
        )
    
    def reset(self):
        self.simulation.reset_simulation()
        base_path, had_good_preliminary, had_good_fda = get_base_path(
            initial_price=self.initial_price, 
            up_price=self.up_price, 
            down_price=self.down_price, 
            total_ticks=self.total_steps, 
            extra_ticks=self.extra_steps + 1, 
            preliminary_probability=0.5, 
            fda_probability=0.75, 
            rng=self.rng
        )
        self.base_path = base_path
        
        self.news: dict[int, Tuple[int, str]] = {}
        self.news[500] = self.rng.choice(positive_preliminary_blurbs) if had_good_preliminary else self.rng.choice(negative_preliminary_blurbs)
        self.news[900] = self.rng.choice(positive_fda_blurbs) if had_good_fda else self.rng.choice(negative_fda_blurbs)
        
        self.stock_midpoints: List[float] = []
        self.removal_percentage = 0.1
        pass
    
    pass

current_case = SimulationBiotech()
case_state = SimulationState.paused
client_id_to_socket: dict[int, websockets.WebSocketServerProtocol] = {}
client_id_to_user_id: dict[int, int] = {}

async def step_loop():
    while True:
        if case_state == SimulationState.running:
            try:
                is_done, result = current_case.step()
                
                for (client_id, socket) in client_id_to_socket.items():
                    user_id = client_id_to_user_id[client_id]
                    
                    if result.tick - 1 == 0:
                        ob = [
                            (ticker, current_case.simulation.get_order_book(current_case.simulation.get_security_id(ticker)))
                            for ticker in current_case.simulation.get_all_tickers()
                        ]
                        kob = {ticker: OrderBook(
                            bid=[limit_order_convert(l) for l in book[0]],
                            ask=[limit_order_convert(l) for l in book[1]]    
                        ) for ticker, book in ob}
                        por = current_case.simulation.get_user_portfolio(user_id)
                        pok = {ticker: por[current_case.simulation.get_security_id(ticker)] for ticker in current_case.simulation.get_all_tickers()}
                        
                        msg = MessageSimulationLoad(
                            simulation_state=case_state,
                            tick=result.tick - 1,
                            max_tick=current_case.simulation.get_N(),
                            all_securities=current_case.simulation.get_all_tickers(),
                            tradeable_securities=current_case.simulation.get_all_tickers(),
                            security_info={ticker: SecurityInfo(
                                security_id=current_case.simulation.get_security_id(ticker),
                                decimal_places=2,
                                net_limit=100,
                                gross_limit=100,
                                max_trade_volume=20
                            ) for ticker in current_case.simulation.get_all_tickers()},
                            order_book_per_security=kob,
                            transactions={ticker: [] for ticker in current_case.simulation.get_all_tickers()},
                            user_id_to_username=current_case.simulation.get_user_id_to_username(),
                            portfolio=pok,
                            news=[]
                        )
                        pass
                    else:
                        msg = MessageMarketUpdate(
                            tick=result.tick - 1,
                            submitted_orders=result.submitted_orders,
                            cancelled_orders=result.cancelled_orders,
                            transacted_orders=result.transacted_orders,
                            order_book_per_security=result.order_book_per_security,
                            portfolio={current_case.simulation.get_security_ticker(index): holdings for index, holdings in enumerate(result.portfolios[user_id])},
                            new_news=[],
                        )
                        pass
                    
                    await socket.send(json.dumps(msg, cls=EnhancedJSONEncoder))
                    pass
                
                if is_done:
                    current_case.reset()
                pass
            except Exception as e:
                print(f"[STEP LOOP] Encountered exception: {e}") 
        
        tick = current_case.get_tick()
        if (
            False and
            (999 <= tick <= 1001 or 0 <= tick <= 5)
        ):
            await asyncio.sleep(10.0)
            pass
        await asyncio.sleep(1.0/4.0)

async def handle_client(websocket: websockets.WebSocketServerProtocol, path: str):
    client_id = id(websocket)
    client_id_to_socket[client_id] = websocket
    print(f"[Server] Client {client_id} connected.")
    
    try:
        user_id = current_case.register_user(f"USER-{client_id}")
        client_id_to_user_id[client_id] = user_id
        
        await websocket.send(json.dumps(MessageLoginResponse(user_id=user_id), cls=EnhancedJSONEncoder))

        ob = [
            (ticker, current_case.simulation.get_order_book(current_case.simulation.get_security_id(ticker)))
            for ticker in current_case.simulation.get_all_tickers()
        ]
        kob = {ticker: OrderBook(
            bid=[limit_order_convert(l) for l in book[0]],
            ask=[limit_order_convert(l) for l in book[1]]    
        ) for ticker, book in ob}
        por = current_case.simulation.get_user_portfolio(user_id)
        pok = {ticker: por[current_case.simulation.get_security_id(ticker)] for ticker in current_case.simulation.get_all_tickers()}
        
        load = MessageSimulationLoad(
            simulation_state=case_state,
            tick=current_case.get_tick(),
            max_tick=current_case.simulation.get_N(),
            all_securities=current_case.simulation.get_all_tickers(),
            tradeable_securities=current_case.simulation.get_all_tickers(),
            security_info={ticker: SecurityInfo(
                security_id=current_case.simulation.get_security_id(ticker),
                decimal_places=2,
                net_limit=100,
                gross_limit=100,
                max_trade_volume=20
            ) for ticker in current_case.simulation.get_all_tickers()},
            order_book_per_security=kob,
            transactions={ticker: [] for ticker in current_case.simulation.get_all_tickers()},
            user_id_to_username=current_case.simulation.get_user_id_to_username(),
            portfolio=pok,
            news=[]
        )
        
        await websocket.send(json.dumps(load, cls=EnhancedJSONEncoder))
        
        async for message in websocket:
            if case_state == SimulationState.running:
                current_case.handle_message(message, websocket)
                pass
    except websockets.exceptions.ConnectionClosed:
        print(f"[Server] Client {client_id} disconnected.")
    except Exception as e:
        print(f"[Server] Received exception from {client_id}: {e}")
    finally:
        del client_id_to_socket[client_id]

async def start_command():
    global case_state
    if not case_state == SimulationState.running:
        print("[Server] Simulation started.")
        case_state = SimulationState.running
        for socket in client_id_to_socket.values():
            await socket.send(json.dumps(MessageSimulationUpdate(
                tick=current_case.get_tick(),
                simulation_state=case_state,
            ), cls=EnhancedJSONEncoder))
            pass
    else:
        print("[Server] Simulation is already running.")
    pass

async def pause_command():
    global case_state
    if case_state == SimulationState.running:
        print("[Server] Simulation paused.")
        case_state = SimulationState.paused
        for socket in client_id_to_socket.values():
            await socket.send(json.dumps(MessageSimulationUpdate(
                tick=current_case.get_tick(),
                simulation_state=case_state,
            ), cls=EnhancedJSONEncoder))
            pass
    else:
        print("[Server] Simulation is not running.")
    pass

async def terminal_loop():
    loop = asyncio.get_running_loop()
    while True:
        command = await loop.run_in_executor(None, input, ">>> ")
        command = command.strip().lower()
        match command:
            case "start":
                await start_command()
                pass
            case "pause":
                await pause_command()
                pass
            case _:
                print("[Server] Unknown command.")
                pass

async def main():
    server = await websockets.serve(handle_client, "localhost", 8765)
    print("[Server] WebSocket server started at ws://localhost:8765")
    await asyncio.gather(
        terminal_loop(),
        step_loop(),
    )
    
if __name__ == "__main__":
    asyncio.run(main())
    pass