import asyncio
import websockets
import json
import sys

URI = "ws://localhost:8765"

async def receive_messages(websocket):
    try:
        async for message in websocket:
            if isinstance(message, bytes):
                sys.stdout.write(f"\r[Server]: <Received Audio Chunk {len(message)} bytes>\n> ")
                sys.stdout.flush()
            else:
                try:
                    data = json.loads(message)
                    if data.get("type") == "emotion":
                        sys.stdout.write(f"\r[Server]: Emotion -> {data['emotion']}\n> ")
                    else:
                        sys.stdout.write(f"\r[Server]: {message}\n> ")
                except:
                    sys.stdout.write(f"\r[Server]: {message}\n> ")
                sys.stdout.flush()
    except websockets.exceptions.ConnectionClosed:
        print("\nDisconnected from server.")

async def send_messages(websocket):
    print("Connected to Robot Server. Type your message and press Enter.")
    print("Type 'exit' to quit.")
    
    loop = asyncio.get_running_loop()
    
    while True:
        sys.stdout.write("> ")
        sys.stdout.flush()
        
        text = await loop.run_in_executor(None, sys.stdin.readline)
        text = text.strip()
        
        if not text:
            continue
            
        if text.lower() == "exit":
             break
            
        payload = {
            "type": "text",
            "text": text
        }
        await websocket.send(json.dumps(payload))

async def main():
    try:
        async with websockets.connect(URI) as websocket:
            receive_task = asyncio.create_task(receive_messages(websocket))
            send_task = asyncio.create_task(send_messages(websocket))
            
            await asyncio.wait(
                [receive_task, send_task],
                return_when=asyncio.FIRST_COMPLETED,
            )
    except ConnectionRefusedError:
        print(f"Could not connect to {URI}. Is the server running?")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nExiting...")
