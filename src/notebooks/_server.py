import asyncio
from typing import Tuple
import websockets
from websockets import WebSocketServerProtocol
import numpy as np
import python_modules.Server as Server

class SimulationBiotech:
    def __init__(self):
        self.rng = np.random.default_rng()
        self.news: dict[int, Tuple[int, str]] = {}
        
        self.total_steps = 1_000
        self.currency = Server.GenericSecurities.GenericCurrency("CAD")
        self.stock = Server.GenericSecurities.GenericStock("BIOTECH", "CAD")
        self.simulation = Server.GenericSimulation(
            { "CAD": self.currency, "BIOTECH": self.stock }, 1.0, self.total_steps
        )
        self.currency_id = self.simulation.get_security_ticker("CAD")
        self.stock_id = self.simulation.get_security_ticker("BIOTECH")
        self.anon_id = self.simulation.add_user("AGENT")
        
        self.reset()
        pass
    
    def register_user(self, username: str) -> int:
        return self.simulation.add_user(username)
    
    def reset(self):
        self.simulation.reset_simulation()
        self.news: dict[int, Tuple[int, str]] = {}
            
        # Preliminary result | FDA Decision | Probability | Final Price
        # GOOD (50%) ($125)  | ACCEPT (75%) | 37.5%       | $150
        # GOOD (50%) ($125)  | REJECT (25%) | 12.5%       | $50
        # BAD  (50%) ($75)   | ACCEPT (25%) | 12.5%       | $150
        # BAD  (50%) ($75)   | REJECT (75%) | 37.5%       | $50
        
        # Start price at $100
        # from step 0 to 200, random drift
        # from step 200 to 400 increased volatility
        # from 400 to 500, increased volatility
        # At 500, get preliminary results
        # from 500 to 800, decreased volatility
        # from 800 to 900, increased volatility
        # At 900, FDA releases decision
        # From 900 to 1000 convergence to true price (either 50 or 150), low volatility
        
        
        
        pass
    pass

class Simulation:
    def __init__(self):
        self.running = False

    async def step(self):
        print("[Simulation] Step executed.")
        await broadcast("[Server] Simulation step occurred.")

    async def handle_message(self, client_id: str, message: str):
        print(f"[Simulation] Received from {client_id}: {message}")
        # You could parse and act on the message here

# Shared global state
clients = {}  # client_id -> websocket
simulation = Simulation()
simulation_task = None

async def broadcast(message: str):
    for client in clients.values():
        await client.send(message)

async def step_loop():
    while True:
        if simulation.running:
            await simulation.step()
        await asyncio.sleep(1)

async def handle_client(websocket: WebSocketServerProtocol, path: str):
    client_id = str(id(websocket))
    clients[client_id] = websocket
    print(f"[Server] Client {client_id} connected.")
    await websocket.send("[Server] Connected.")

    try:
        async for message in websocket:
            if simulation.running:
                await simulation.handle_message(client_id, message)
    except websockets.exceptions.ConnectionClosed:
        print(f"[Server] Client {client_id} disconnected.")
    finally:
        del clients[client_id]

async def terminal_loop():
    loop = asyncio.get_running_loop()
    while True:
        command = await loop.run_in_executor(None, input, ">>> ")
        command = command.strip().lower()
        if command == "start":
            if not simulation.running:
                simulation.running = True
                await broadcast("[Server] Simulation started.")
                print("[Server] Simulation started.")
            else:
                print("[Server] Simulation is already running.")
        elif command == "pause":
            if simulation.running:
                simulation.running = False
                await broadcast("[Server] Simulation paused.")
                print("[Server] Simulation paused.")
            else:
                print("[Server] Simulation is not running.")
        elif command == "users":
            print(f"[Server] {clients.keys()}")
        else:
            print("[Server] Unknown command.")

async def main():
    server = await websockets.serve(handle_client, "localhost", 8765)
    print("[Server] WebSocket server started at ws://localhost:8765")
    await asyncio.gather(
        terminal_loop(),
        step_loop(),
    )

if __name__ == "__main__":
    asyncio.run(main())
