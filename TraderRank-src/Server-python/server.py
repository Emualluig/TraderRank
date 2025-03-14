import asyncio
from dataclasses import dataclass
import websockets

# Store all connected client connections
connected_clients = set()

async def broadcast(message: str):
    """
    Broadcast a message to all connected clients.
    """
    if connected_clients:  # Only proceed if there is at least one client
        await asyncio.wait([client.send(message) for client in connected_clients])

async def handle_client(websocket, path):
    """
    Handle a new WebSocket client connection. Keep listening for
    messages until the client disconnects or an error occurs.
    """
    # Register new client connection
    connected_clients.add(websocket)
    print(f"Client connected. Total clients: {len(connected_clients)}")

    try:
        # Continuously listen for messages from the client
        async for message in websocket:
            print(f"Received from client: {message}")
            
            # Example echo back to the single client
            response = f"You sent: {message}"
            await websocket.send(response)
            
            # Optionally broadcast the message to all clients
            # await broadcast(f"Broadcasting: {message}")

    except websockets.exceptions.ConnectionClosedError:
        # Handle abrupt client disconnection
        print("Client disconnected abruptly.")

    finally:
        # Unregister client (always do in finally to handle normal or abrupt closure)
        connected_clients.remove(websocket)
        print(f"Client disconnected. Total clients: {len(connected_clients)}")

async def main(port: int):
    """
    Main entry point to start the WebSocket server.
    """
    # Create WebSocket server
    async with websockets.serve(handle_client, "localhost", port):
        print("WebSocket server started on ws://localhost:8765")
        # Keep the server running until interrupted
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    asyncio.run(main(5678))