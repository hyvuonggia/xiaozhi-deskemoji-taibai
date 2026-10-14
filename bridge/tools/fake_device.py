"""Thiet bi gia (fake device) noi vao xiaozhi-server de do do tre end-to-end.
Gui 1 cau hoi bang Opus (nhu robot that), roi do: luc gui 'listen stop' -> khung audio TTS dau tien.
Chay TRONG container: docker exec -w /opt/xiaozhi-esp32-server xiaozhi-server python /tmp/fake_device.py <wav>
"""
import asyncio
import json
import sys
import time
import uuid
import wave

import numpy as np
import opuslib_next
import websockets

SERVER = "ws://127.0.0.1:8000/xiaozhi/v1/"
DEVICE_ID = "28:84:85:65:2b:2c"
CLIENT_ID = str(uuid.uuid4())

TOOLS = [
    {"name": "self.get_device_status", "description": "Provides the real-time information of the device.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "self.audio_speaker.set_volume", "description": "Set the volume of the audio speaker.",
     "inputSchema": {"type": "object", "properties": {"volume": {"type": "integer"}}, "required": ["volume"]}},
    {"name": "self.head.look_left", "description": "Turn the robot head to its left and look that way.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "self.head.look_right", "description": "Turn the robot head to its right and look that way.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "self.head.nod", "description": "Move the head up and down (nodding): agreement, greeting or saying yes.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "self.head.shake", "description": "Move the head left and right (shaking): disagreement, refusal or saying no.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "self.head.center", "description": "Return the head to the straight, neutral position.",
     "inputSchema": {"type": "object", "properties": {}}},
]


def load16(path):
    with wave.open(path, "rb") as w:
        sr, ch, sw = w.getframerate(), w.getnchannels(), w.getsampwidth()
        raw = w.readframes(w.getnframes())
    a = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    if ch > 1:
        a = a.reshape(-1, ch).mean(axis=1)
    if sr != 16000:
        n = int(round(len(a) * 16000 / sr))
        a = np.interp(np.linspace(0, len(a) - 1, n), np.arange(len(a)), a).astype(np.float32)
    return a


async def main():
    wav = sys.argv[1] if len(sys.argv) > 1 else "/opt/xiaozhi-esp32-server/data/tmp/q_gas.wav"
    audio = load16(wav)
    pcm = (audio * 32768.0).astype(np.int16)
    enc = opuslib_next.Encoder(16000, 1, opuslib_next.APPLICATION_AUDIO)
    frames = [pcm[i:i + 960] for i in range(0, len(pcm), 960)]
    print("[fake] cau hoi %.1f s -> %d khung opus 60ms" % (len(audio) / 16000.0, len(frames)))

    headers = {"Device-Id": DEVICE_ID, "Client-Id": CLIENT_ID, "Protocol-Version": "1"}
    t_start = time.time()
    async with websockets.connect(SERVER, additional_headers=headers, max_size=None) as ws:
        await ws.send(json.dumps({"type": "hello", "version": 1, "features": {"mcp": True},
                                  "transport": "websocket",
                                  "audio_params": {"format": "opus", "sample_rate": 16000,
                                                   "channels": 1, "frame_duration": 60}}))

        async def reader(tag):
            while True:
                msg = await ws.recv()
                dt = time.time() - t_stop
                if isinstance(msg, (bytes, bytearray)):
                    if tag == "reply":
                        print("  +%.2fs  <AUDIO %d bytes>  <-- TTS bat dau" % (dt, len(msg)))
                        return dt
                    continue
                try:
                    data = json.loads(msg)
                except Exception:
                    continue
                t = data.get("type")
                if t == "mcp":
                    payload = data.get("payload", {})
                    print("  +%.2fs  << mcp method=%s id=%s" % (dt, payload.get("method"), payload.get("id")))
                    if payload.get("method") == "initialize":
                        await ws.send(json.dumps({"type": "mcp", "payload": {"jsonrpc": "2.0", "id": payload.get("id", 1),
                            "result": {"protocolVersion": "2024-11-05", "capabilities": {"tools": {}},
                                       "serverInfo": {"name": "esp32-s3n16r8-emoji", "version": "1.9.0"}}}}))
                    elif payload.get("method") == "tools/list":
                        await ws.send(json.dumps({"type": "mcp", "payload": {"jsonrpc": "2.0", "id": payload.get("id", 2),
                            "result": {"tools": TOOLS}}}))
                    elif payload.get("method") == "tools/call":
                        params = payload.get("params", {})
                        tname = params.get("name")
                        print("  +%.2fs  >> THIET BI DUOC YEU CAU CHAY: %s %s" % (dt, tname, json.dumps(params.get("arguments", {}), ensure_ascii=False)))
                        await ws.send(json.dumps({"type": "mcp", "payload": {"jsonrpc": "2.0", "id": payload.get("id"),
                            "result": {"content": [{"type": "text", "text": "ok"}], "isError": False}}}))
                    continue
                if tag == "reply":
                    if t == "tts":
                        print("  +%.2fs  JSON tts: %s" % (dt, json.dumps(data, ensure_ascii=False)[:160]))
                        if data.get("state") == "stop":
                            return None
                    elif t in ("stt", "llm", "intent"):
                        print("  +%.2fs  JSON %s: %s" % (dt, t, json.dumps(data, ensure_ascii=False)[:160]))
                    else:
                        print("  +%.2fs  JSON %s: %s" % (dt, t, json.dumps(data, ensure_ascii=False)[:120]))

        task = asyncio.create_task(reader("early"))
        await asyncio.sleep(3.0)  # doi xiaozhi-server nhan xong danh sach cong cu cua thiet bi (MCP handshake)
        await ws.send(json.dumps({"type": "listen", "state": "start", "mode": "auto"}))
        repeats = int(sys.argv[2]) if len(sys.argv) > 2 else 2
        for it in range(1, repeats + 1):
            print("\n===== LUOT NOI %d/%d =====" % (it, repeats))
            for f in frames:
                await ws.send(enc.encode(f.tobytes(), 960))
                await asyncio.sleep(0.06)  # gui theo thoi gian THUC (60 ms/khung) nhu thiet bi that
            t_stop = time.time()
            await ws.send(json.dumps({"type": "listen", "state": "stop"}))
            print("[fake] da gui 'listen stop' tai t=0 (audio %.1fs)" % (t_stop - t_start))
            try:
                dt = await asyncio.wait_for(reader("reply"), timeout=45)
                print("==> LUOT %d: tu luc ket thuc noi den khung am thanh tra loi dau tien: %.2f s" % (it, dt))
            except asyncio.TimeoutError:
                print("==> LUOT %d: KHONG nhan duoc am thanh tra loi trong 45 s" % it)
            await asyncio.sleep(5.0)
        task.cancel()


t_stop = 0.0
asyncio.run(main())
