@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   EVA Robot - Automated Setup Script
echo ========================================

cd /d %~dp0

if exist venv goto :skip_venv
echo [1/3] Creating virtual environment
python -m venv venv
:skip_venv
echo [1/3] Virtual environment ready

echo [2/3] Installing Torch (CUDA) - Forced Reinstall
cmd /c venv\Scripts\python.exe -m pip install --upgrade --force-reinstall torch torchaudio torchvision --index-url https://download.pytorch.org/whl/cu121

echo [2/3] Installing other dependencies
cmd /c venv\Scripts\python.exe -m pip install -r requirements.txt

if exist bin\llama-server.exe goto :skip_llama
echo [3/3] Downloading llama.cpp binaries
if not exist bin mkdir bin
curl -L https://github.com/ggml-org/llama.cpp/releases/download/b8429/llama-b8429-bin-win-cuda-12.4-x64.zip -o llama_bin.zip
tar -xf llama_bin.zip -C bin
del llama_bin.zip
echo Binaries installed in server\bin
goto :setup_end

:skip_llama
echo [3/3] llama.cpp binaries already installed

:setup_end
echo.
echo ========================================
echo   Setup Complete! 
echo ========================================
echo.
echo IMPORTANT: To start the robot:
echo 1. Ensure you have the models in the root directory:
echo    - Qwen3-VL-4B-Instruct-Q4_K_M.gguf
echo    - mmproj-Qwen3VL-4B-Instruct-Q8_0.gguf
echo.
echo 2. Run run.bat
echo.
pause
