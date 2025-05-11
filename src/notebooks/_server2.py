from abc import ABC, abstractmethod
import asyncio
from dataclasses import dataclass
import dataclasses
import json
from typing import Tuple, Union
import websockets

@dataclass
class LimitOrderDC:
    pass

@dataclass
class BidAskBook:
    bids: list[LimitOrderDC]
    asks: list[LimitOrderDC]
    pass

@dataclass
class BaseMessage:
    type_: str
    pass

@dataclass
class SyncMessage(BaseMessage):
    type_: str = "sync"
    client_id: int
    security_to_id: dict[str, int]
    id_to_security: dict[int, str]
    tradable_securities_id: list[str]
    user_portfolio: dict[int, float]
    username_to_user_id: dict[str, int]
    order_book_per_security: dict[str, BidAskBook]
    pass
    
@dataclass
class UpdateMessage(BaseMessage):
    type_: str = "update"
    user_portfolio: dict[int, float]
    username_to_user_id: dict[str, int]
    order_book_per_security: dict[str, BidAskBook]
    pass

class BaseCase(ABC):
    @abstractmethod
    def on_user_connect(self, username: str) -> SyncMessage:
        pass
    
    @abstractmethod
    def on_step(self) -> Tuple[UpdateMessage, bool]:
        pass
    
    @abstractmethod
    def handle_message(self, message: str, websocket: websockets.WebSocketServerProtocol):
        pass
    pass

class BiotechCase(BaseCase):
    def on_user_connect(self, username) -> SyncMessage:
        raise NotImplementedError

    def on_step(self) -> UpdateMessage:
        raise NotImplementedError
    
    def handle_message(self, message, websocket):
        raise NotImplementedError

class EnhancedJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)     
        return super().default(o)

async def broadcast(message: Union[SyncMessage, UpdateMessage]):
    for client in client_id_to_socket.values():
        await client.send(json.dumps(message, cls=EnhancedJSONEncoder))

async def send(websocket: websockets.WebSocketServerProtocol, message: Union[SyncMessage, UpdateMessage]):
    await websocket.send(json.dumps(message, cls=EnhancedJSONEncoder))

current_case = BiotechCase()
is_case_running = False
client_id_to_socket: dict[int, websockets.WebSocketServerProtocol] = {}
client_id_to_user_id: dict[int, int] = {}

async def step_loop():
    while True:
        if is_case_running:
            try:
                update_message, is_finished = current_case.on_step()
                await broadcast(update_message)
            except Exception as e:
                print(f"[STEP LOOP] Encountered exception: {e}") 
        await asyncio.sleep(0.25)

async def handle_client(websocket: websockets.WebSocketServerProtocol, path: str):
    client_id = id(websocket)
    client_id_to_socket[client_id] = websocket
    print(f"[Server] Client {client_id} connected.")
    
    try:
        sync_message = current_case.on_user_connect(f"USER-{client_id}")
        client_id_to_user_id[client_id] = sync_message.client_id
        
        await send(sync_message)
        async for message in websocket:
            if is_case_running:
                current_case.handle_message(message, websocket)
    except websockets.exceptions.ConnectionClosed:
        print(f"[Server] Client {client_id} disconnected.")
    except Exception as e:
        print(f"[Server] Received exception from {client_id}: {e}")
    finally:
        del client_id_to_socket[client_id]

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
            case "users":
                await users_command()
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