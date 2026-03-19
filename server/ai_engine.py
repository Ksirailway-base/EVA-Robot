import os
import torch
import requests
import json

try:
    from faster_whisper import WhisperModel
except ImportError:
    WhisperModel = None


class AIEngine:
    """
    Offline AI Engine for the EVA Robot.
    Connects to llama-server running locally (started by run.bat).
    No openai package needed - uses requests directly.
    """
    def __init__(self, model_path, local_server_url="http://localhost:8080/v1"):
        self.model_path = model_path
        self.local_server_url = local_server_url
        self.stt = None
        self.chat_history = []
        self.system_prompt = self.load_system_prompt()

    def load_system_prompt(self):
        prompt_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "system_prompt.txt")
        default_prompt = (
            "You are a helpful robot companion. Your goal is to be concise, clear, and friendly.\n"
            "RULES:\n"
            "1. ALWAYS start your response with an emotion tag: [NEUTRAL], [HAPPY], [SAD], or [ANGRY].\n"
            "2. DO NOT use Markdown.\n"
            "3. Respond ONLY with plain text."
        )
        try:
            if os.path.exists(prompt_path):
                with open(prompt_path, "r", encoding="utf-8") as f:
                    print(f"Loading system prompt from {prompt_path}")
                    return f.read().strip()
            return default_prompt
        except Exception as e:
            print(f"Prompt load error: {e}")
            return default_prompt

    def load_models(self):
        print("--- Initializing Local AI Modules ---")
        self._load_whisper()
        print(f"LLM: connecting to llama-server at {self.local_server_url}")

    def _load_whisper(self):
        if not WhisperModel:
            print("faster-whisper not installed. STT unavailable.")
            print("  -> pip install faster-whisper")
            return
        try:
            device = "cuda" if torch.cuda.is_available() else "cpu"
            print(f"Initializing Whisper on {device}...")
            self.stt = WhisperModel(
                "small",
                device=device,
                compute_type="float16" if device == "cuda" else "int8"
            )
            print("Whisper ready.")
        except Exception as e:
            print(f"Whisper load failed: {e}")

    def transcribe(self, audio_path):
        if not self.stt:
            return ""
        try:
            segments, _ = self.stt.transcribe(audio_path, beam_size=5)
            return " ".join([s.text for s in segments]).strip()
        except Exception as e:
            print(f"Transcription error: {e}")
            return ""

    def generate_response(self, text, image_base64=None):
        messages = [{"role": "system", "content": self.system_prompt}]
        messages.extend(self.chat_history[-10:])

        if image_base64:
            content = [
                {"type": "image_url", "image_url": {"url": f"data:image/jpeg;base64,{image_base64}"}},
                {"type": "text", "text": text},
            ]
        else:
            content = [{"type": "text", "text": text}]

        messages.append({"role": "user", "content": content})

        try:
            response = requests.post(
                f"{self.local_server_url}/chat/completions",
                headers={"Content-Type": "application/json"},
                json={
                    "model": "local",
                    "messages": messages,
                    "max_tokens": 150,
                    "temperature": 0.7,
                },
                timeout=60
            )
            response.raise_for_status()
            reply = response.json()["choices"][0]["message"]["content"]

            self.chat_history.append({"role": "user", "content": text})
            self.chat_history.append({"role": "assistant", "content": reply})
            return reply

        except requests.exceptions.ConnectionError:
            print("Cannot connect to llama-server. Is it running?")
            return "[NEUTRAL] I cannot connect to my brain. Is llama-server running?"
        except Exception as e:
            print(f"LLM inference error: {e}")
            return "[NEUTRAL] I encountered a thinking error. Please check the logs."

    def clear_history(self):
        self.chat_history = []
        print("Chat history cleared.")

    def warmup_whisper(self):
        pass