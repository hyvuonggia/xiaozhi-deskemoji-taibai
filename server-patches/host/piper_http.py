#!/usr/bin/env python3
"""Local Piper HTTP adapter for Xiaozhi CustomTTS."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from io import BytesIO
import json
import os
import threading
import wave

from piper import PiperVoice

MODEL = os.environ.get("PIPER_MODEL", "/opt/xiaozhi-esp32-server/models/piper/vi_VN-vais1000-medium.onnx")
CONFIG = os.environ.get("PIPER_CONFIG", MODEL + ".json")
HOST = os.environ.get("PIPER_HOST", "172.17.0.1")
PORT = int(os.environ.get("PIPER_PORT", "8879"))
voice = PiperVoice.load(MODEL, config_path=CONFIG)
LENGTH_SCALE = float(os.environ.get("PIPER_LENGTH_SCALE", "1.15"))
from piper.config import SynthesisConfig
SYN_CONFIG = SynthesisConfig(length_scale=LENGTH_SCALE)
synthesis_lock = threading.Lock()

class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print("piper-http: " + fmt % args, flush=True)

    def do_GET(self):
        if self.path != "/health":
            self.send_error(404)
            return
        payload = json.dumps({"ok": True, "voice": "vi_VN-vais1000-medium"}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_POST(self):
        if self.path != "/tts":
            self.send_error(404)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length < 1 or length > 8192:
                self.send_error(400, "request body must be between 1 and 8192 bytes")
                return
            body = json.loads(self.rfile.read(length))
            text = body.get("text") or body.get("input")
            if not isinstance(text, str) or not text.strip() or len(text) > 2000:
                self.send_error(400, "text is required (max 2000 chars)")
                return
            audio = BytesIO()
            with synthesis_lock:
                with wave.open(audio, "wb") as wav:
                    voice.synthesize_wav(text.strip(), wav, syn_config=SYN_CONFIG)
            data = audio.getvalue()
            self.send_response(200)
            self.send_header("Content-Type", "audio/wav")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        except (ValueError, json.JSONDecodeError) as exc:
            self.send_error(400, str(exc))
        except Exception as exc:
            print(f"piper-http synthesis error: {exc!r}", flush=True)
            self.send_error(500, "speech synthesis failed")

if __name__ == "__main__":
    print(f"Loading Piper voice {MODEL}", flush=True)
    ThreadingHTTPServer((HOST, PORT), Handler).serve_forever()
