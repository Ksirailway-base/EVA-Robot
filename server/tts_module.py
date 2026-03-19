import os
import asyncio
import logging
import tempfile
import time
import warnings
from importlib.resources import files

warnings.filterwarnings("ignore", category=FutureWarning)
try:
    from transformers import logging as transformers_logging
    transformers_logging.set_verbosity_error()
except ImportError:
    pass

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class F5TTSHandler:
    def __init__(self):
        base_dir = os.path.join(os.path.dirname(__file__), "ref_audio")
        MANUAL_REF_FILE = "custom.wav" 

        if MANUAL_REF_FILE:
            self.f5_ref_audio = os.path.join(base_dir, MANUAL_REF_FILE)
            logger.info(f"Using manual reference audio: {MANUAL_REF_FILE}")
        else:
            priorities = ["custom.wav", "custom.WAV", "custom2.mp3", "custom2.wav", "custom.mp3"]
            self.f5_ref_audio = os.path.join(base_dir, "custom.wav")
            for p in priorities:
                path = os.path.join(base_dir, p)
                if os.path.exists(path):
                    self.f5_ref_audio = path
                    logger.info(f"Selected reference audio: {p}")
                    break

        self.f5_ref_text_path = self.f5_ref_audio + ".txt"
        self.f5_ref_text = "" 
        
        if os.path.exists(self.f5_ref_text_path):
            try:
                with open(self.f5_ref_text_path, "r", encoding="utf-8") as f:
                    self.f5_ref_text = f.read().strip()
                logger.info(f"Loaded cached reference text: {self.f5_ref_text[:30]}...")
            except Exception as e:
                logger.warning(f"Failed to load cached ref text: {e}")

        self.ref_lang = self.detect_language(self.f5_ref_text)
        logger.info(f"Reference text language detected as: {self.ref_lang}")

        self.f5_instance = None
        self.model_loaded = False
        
        if not os.path.exists(self.f5_ref_audio):
            logger.warning(f"Reference voice file not found at {self.f5_ref_audio}.")

    def detect_language(self, text):
        if not text:
            return "unknown"
        cyrillic_chars = sum(1 for char in text if '\u0400' <= char <= '\u04FF')
        total_chars = len([c for c in text if c.isalpha()])
        if total_chars > 0 and (cyrillic_chars / total_chars) > 0.5:
            return "ru"
        return "en"

    def load_model(self):
        if self.model_loaded and self.f5_instance:
            return

        try:
            logger.info("Initializing F5-TTS model...")
            start_time = time.time()
            from f5_tts.api import F5TTS
            import torch
            device = "cuda" if torch.cuda.is_available() else "cpu"
            logger.info(f"Using device: {device}")
            
            model_name = "F5TTS_v1_Base"
            self.f5_instance = F5TTS(model=model_name, device=device)
            self.model_loaded = True
            elapsed = time.time() - start_time
            logger.info(f"F5-TTS model initialized in {elapsed:.2f}s.")
        except Exception as e:
            logger.error(f"Failed to load F5-TTS model: {e}")
            self.f5_instance = None
            self.model_loaded = False
            raise e

    async def warmup(self):
        if not self.model_loaded:
            self.load_model()

        if not self.f5_instance:
            logger.error("Cannot warm up: Model not loaded.")
            return

        logger.info("Starting F5-TTS warmup...")
        try:
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
                warmup_output = tmp.name
            
            warmup_text = "Systems are online, I am ready."
            
            def run_inference():
                ref_audio = self.f5_ref_audio
                if not os.path.exists(ref_audio):
                     return

                if not self.f5_ref_text:
                    try:
                        self.f5_ref_text = self.f5_instance.transcribe(ref_audio).strip()
                        with open(self.f5_ref_text_path, "w", encoding="utf-8") as f:
                            f.write(self.f5_ref_text)
                    except Exception as e:
                        logger.warning(f"Failed to cache transcription: {e}")

                self.f5_instance.infer(
                    ref_file=ref_audio,
                    ref_text=self.f5_ref_text,
                    gen_text=warmup_text,
                    file_wave=warmup_output
                )
            
            start_time = time.time()
            await asyncio.to_thread(run_inference)
            elapsed = time.time() - start_time
            
            if os.path.exists(warmup_output):
                os.remove(warmup_output)
                logger.info(f"F5-TTS warmup complete in {elapsed:.2f}s.")
            else:
                logger.error("F5-TTS warmup failed: No output generated.")
        except Exception as e:
            logger.error(f"F5-TTS warmup error: {e}")

    async def generate_audio(self, text, output_file):
        if not self.f5_instance:
            try:
                self.load_model()
            except:
                return None

        if not os.path.exists(self.f5_ref_audio):
             return None

        try:
            def run_inference():
                if not self.f5_ref_text:
                     if os.path.exists(self.f5_ref_text_path):
                         with open(self.f5_ref_text_path, "r", encoding="utf-8") as f:
                             self.f5_ref_text = f.read().strip()
                
                import re
                clean_text = re.sub(r'\s+', ' ', text).strip()
                clean_text = re.sub(r'[*\(\)\[\]]', '', clean_text)

                self.f5_instance.infer(
                    ref_file=self.f5_ref_audio,
                    ref_text=self.f5_ref_text,
                    gen_text=clean_text,
                    file_wave=output_file,
                    nfe_step=32,
                    cfg_strength=2.0,
                    sway_sampling_coef=-1,
                    speed=1.0,
                    remove_silence=False
                )
            
            await asyncio.to_thread(run_inference)
            return output_file if os.path.exists(output_file) else None
        except Exception as e:
            logger.error(f"F5-TTS generation failed: {e}")
            return None
