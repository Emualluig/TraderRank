import asyncio
import websockets

async def client():
    uri = "ws://localhost:8765"
    async with websockets.connect(uri) as websocket:
        while True:
            msg = await websocket.recv()
            print(msg)

if __name__ == "__main__":
    asyncio.run(client())
