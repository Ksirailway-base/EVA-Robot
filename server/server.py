import asyncio
import websockets
import json
import wave
import time
import os
import struct
from ai_engine import AIEngine
from vision import VisionModule
from tts_module import F5TTSHandler

HOST = "0.0.0.0"
PORT = 8765
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODEL_PATH = os.path.join(BASE_DIR, "Qwen3-VL-4B-Instruct-Q4_K_M.gguf")
TESTS_DIR = os.path.join(BASE_DIR, "tests")

if not os.path.exists(TESTS_DIR):
    os.makedirs(TESTS_DIR)

TEMP_AUDIO_IN = os.path.join(TESTS_DIR, "input.wav")
TEMP_AUDIO_OUT = os.path.join(TESTS_DIR, "output.wav")

ai = AIEngine(MODEL_PATH)
vision = VisionModule(TESTS_DIR)
tts_handler = F5TTSHandler()

connected_clients = set()
audio_buffer = bytearray()
last_audio_time = 0
IS_PROCESSING = False
WAITING_FOR_IMAGE = False
PENDING_TEXT_REQUEST = ""

async def keep_alive_heartbeat():
    while True:
        try:
            if connected_clients:
                await broadcast_json({"type": "log", "msg": "Server processing..."})
            await asyncio.sleep(2)
        except asyncio.CancelledError:
            break
        except Exception as e:
            print(f"Heartbeat error: {e}")
            await asyncio.sleep(2)

async def process_audio():
    global audio_buffer, IS_PROCESSING
    if IS_PROCESSING:
        print("Still processing previous audio, skipping...")
        return
        
    if len(audio_buffer) < 32000: 
        return

    print("Processing audio...")
    IS_PROCESSING = True
    
    try:
        with wave.open(TEMP_AUDIO_IN, 'wb') as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(16000)
            wf.writeframes(audio_buffer)
        
        audio_buffer = bytearray()
        
        text = ai.transcribe(TEMP_AUDIO_IN)
        print(f"User said: {text}")
        
        hallucinations = [
            "subtitle", "subtitles", "thank you for watching", "thanks for watching",
            "watching video", "mbc news"
        ]
        
        if not text or len(text.strip()) < 2:
            print("Ignored empty/short audio.")
            IS_PROCESSING = False
            return

        text_lower = text.lower().strip()
        is_hallucination = False
        
        for h in hallucinations:
            if h == text_lower:
                is_hallucination = True
                break
        
        if is_hallucination:
            print(f"Ignored potential hallucination: {text}")
            IS_PROCESSING = False
            return

        await process_interaction(text)
        
    except Exception as e:
        print(f"Error in process_audio: {e}")
        IS_PROCESSING = False

async def process_interaction(text):
    global IS_PROCESSING, WAITING_FOR_IMAGE, PENDING_TEXT_REQUEST
    IS_PROCESSING = True
    
    vision_triggers = [
        "what do you see", "what are you seeing", "what is in front of you",
        "look at", "take a look", "describe what you see",
        "camera", "photo", "image", "take a photo", "see"
    ]
    
    is_vision_request = any(trigger in text.lower() for trigger in vision_triggers)
    
    if is_vision_request:
        print("Vision Trigger Detected! Requesting image from ESP32...")
        await broadcast_json({"type": "vision"})
        WAITING_FOR_IMAGE = True
        PENDING_TEXT_REQUEST = text
        return 

    await generate_and_speak(text, None)

async def generate_and_speak(text, image_b64):
    global IS_PROCESSING
    IS_PROCESSING = True
    
    response_text = ai.generate_response(text, image_b64)
    print(f"Bot says: {response_text}")
    
    emotion = "neutral"
    
    import re
    match = re.match(r'^\[(HAPPY|SAD|ANGRY|NEUTRAL)\]\s*', response_text, re.IGNORECASE)
    if match:
        emotion_tag = match.group(1).lower()
        emotion = emotion_tag
        response_text = response_text[match.end():]
    else:
        if "happy" in response_text.lower(): emotion = "happy"
        elif "sad" in response_text.lower(): emotion = "sad"
        elif "angry" in response_text.lower(): emotion = "angry"
    
    await broadcast_json({"type": "emotion", "emotion": emotion})
    
    tts_output = await tts_handler.generate_audio(response_text, TEMP_AUDIO_OUT)
    
    wav_file = None
    if tts_output and os.path.exists(tts_output):
        try:
            from pydub import AudioSegment
            sound = AudioSegment.from_file(tts_output)
            sound = sound.set_frame_rate(16000).set_channels(1).set_sample_width(2)
            
            def match_target_amplitude(sound, target_dBFS):
                change_in_dBFS = target_dBFS - sound.dBFS
                return sound.apply_gain(change_in_dBFS)
            
            sound = match_target_amplitude(sound, -10.0)

            try:
                sound = sound.high_pass_filter(100)
            except Exception:
                pass

            wav_filename = os.path.join(TESTS_DIR, "response_pcm.raw")
            sound.export(wav_filename, format="raw")
            wav_file = wav_filename
            print(f"Converted TTS to PCM RAW. Source: {tts_output}")
        except Exception as e:
            print(f"Error converting audio: {e}")
            pass

    if wav_file:
         await stream_audio(wav_file)
         
    IS_PROCESSING = False

async def stream_audio(file_path):
    if not os.path.exists(file_path):
        return
    
    with open(file_path, "rb") as f:
        data = f.read()
        chunk_size = 2048 
        delay = chunk_size / 32000.0
        
        for i in range(0, len(data), chunk_size):
            chunk = data[i:i+chunk_size]
            to_remove = set()
            if not connected_clients:
                 break

            for ws in list(connected_clients):
                try:
                    await ws.send(chunk)
                except Exception:
                    to_remove.add(ws)
            
            if to_remove:
                connected_clients.difference_update(to_remove)
                
            await asyncio.sleep(delay)

async def broadcast_json(data):
    message = json.dumps(data)
    if connected_clients:
        await asyncio.gather(*(ws.send(message) for ws in connected_clients), return_exceptions=True)

async def handler(websocket):
    global last_audio_time, audio_buffer, WAITING_FOR_IMAGE, PENDING_TEXT_REQUEST
    print("Client connected")
    connected_clients.add(websocket)
    
    try:
        async for message in websocket:
            if isinstance(message, bytes):
                if WAITING_FOR_IMAGE and len(message) > 5000:
                    print(f"Received potential image data: {len(message)} bytes")
                    image_b64 = vision.process_image_data(message)
                    
                    if image_b64:
                        print("Resuming LLM with Image...")
                        WAITING_FOR_IMAGE = False
                        await generate_and_speak(PENDING_TEXT_REQUEST, image_b64)
                    else:
                        print("Failed to process image")
                        WAITING_FOR_IMAGE = False
                else:
                    audio_buffer.extend(message)
                    last_audio_time = time.time()
            else:
                try:
                    data = json.loads(message)
                    msg_type = data.get("type")
                    if msg_type == "text":
                        text_content = data.get('text')
                        print(f"Received text input: {text_content}")
                        
                        if text_content.startswith("/"):
                            cmd_parts = text_content[1:].split()
                            cmd = cmd_parts[0].lower()
                            
                            if cmd == "config" and len(cmd_parts) >= 3:
                                target = cmd_parts[1].lower()
                                value_str = cmd_parts[2].lower()
                                try:
                                    if "." in value_str:
                                        value = float(value_str)
                                    else:
                                        value = int(value_str)
                                except ValueError:
                                    value = value_str == "on"

                                await broadcast_json({
                                    "type": "config", 
                                    target: value
                                })
                                print(f"Sent config: {target} -> {value}")
                            else:
                                print(f"Unknown command: {text_content}")
                        else:
                            await process_interaction(text_content)
                    
                    elif msg_type == "hello":
                        print("Device connected. Sending configuration.")
                        await broadcast_json({
                            "type": "config", 
                            "volume": 1.0
                        })

                    elif msg_type == "end_of_speech":
                        print("PTT Release detected. Processing audio.")
                        await process_audio()

                    elif msg_type == "log":
                        print(f" [Device Log]: {data.get('msg')}")

                    elif msg_type == "error":
                        print(f" [Device Error]: {data.get('msg')}")
                except json.JSONDecodeError:
                    print(f"Received raw message: {message}")
                
    except websockets.exceptions.ConnectionClosed:
        print("Client disconnected")
    finally:
        connected_clients.remove(websocket)

async def main_loop():
    global last_audio_time, IS_PROCESSING
    while True:
        if not IS_PROCESSING and len(audio_buffer) > 0:
            if time.time() - last_audio_time > 1.5:
                await process_audio()
        await asyncio.sleep(0.1)

async def main():
    print("Starting Robot Server...")
    ai.load_models()
    ai.warmup_whisper()
    await tts_handler.warmup()
    
    server = await websockets.serve(handler, HOST, PORT)
    print(f"Server listening on ws://{HOST}:{PORT}")
    
    await asyncio.gather(
        server.wait_closed(),
        main_loop()
    )

if __name__ == "__main__":
    asyncio.run(main())
