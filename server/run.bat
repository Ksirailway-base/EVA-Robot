@echo off
cd /d %~dp0
echo Starting Llama Server in separate window...
start bin\llama-server.exe -m ..\Qwen3-VL-4B-Instruct-Q4_K_M.gguf --mmproj ..\mmproj-Qwen3VL-4B-Instruct-Q8_0.gguf --port 8080 -c 2048 -ngl 100
 
echo Starting EVA Robot Server (Whisper, TTS, and core logic)...
venv\Scripts\python.exe server.py
pause
