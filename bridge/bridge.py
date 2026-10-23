#!/usr/bin/env python3
"""
Hermes <-> Xiaozhi bridge.

Cho phep robot (xiaozhi-server) noi chuyen voi CHINH Hermes agent (bo nao that:
ky uc, skill, tinh cach) VA van dieu khien duoc thiet bi (MCP tool cua firmware:
am luong, servo, ...), thu ma API server cua Hermes khong ho tro truc tiep
(Hermes chay tool cua chinh no, khong nhan danh sach tool cua client).

Cach hoat dong:
  xiaozhi-server --(OpenAI /v1/chat/completions, stream, tools)--> bridge
  bridge        --(hoi Hermes)--> http://127.0.0.1:8642/v1  (agent that)
  Hermes tra ve text, neu can dieu khien thiet bi thi tra ve 1 khoi JSON
  {"tool": "...", "arguments": {...}} -> bridge doi thanh OpenAI tool_calls
  -> xiaozhi-server thuc thi tren mach that (self.audio_speaker.set_volume, ...)
"""
import asyncio
import json
import random
import unicodedata
import os
import re
import time
import uuid
import logging

import aiohttp
from aiohttp import web

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("bridge")

PORT = int(os.environ.get("BRIDGE_PORT", "8643"))
HERMES_BASE = os.environ.get("HERMES_BASE", "http://127.0.0.1:8642/v1")
HERMES_MODEL = os.environ.get("HERMES_MODEL", "hermes-agent")
HERMES_SESSION = os.environ.get("HERMES_SESSION", "robot-deskemoji")
HERMES_TIMEOUT = float(os.environ.get("HERMES_TIMEOUT", "25"))
FALLBACK_TEXT = os.environ.get("BRIDGE_FALLBACK", "Dạ em chưa nghe rõ, sếp nói lại giúp em ạ.")


def load_hermes_key() -> str:
    key = os.environ.get("HERMES_KEY", "").strip()
    if key:
        return key
    env_path = os.path.expanduser("~/.hermes/.env")
    try:
        with open(env_path, "r", encoding="utf-8", errors="ignore") as fh:
            for line in fh:
                if line.startswith("API_SERVER_KEY="):
                    return line.split("=", 1)[1].strip().strip('"').strip("'")
    except OSError:
        pass
    return ""


HERMES_KEY = load_hermes_key()

TOOL_RULES = """Bạn đang là BỘ NÃO của một robot để bàn, nói chuyện với sếp qua loa và mic (giọng nói), \
không phải qua chat chữ. Vì vậy:
LUẬT SỐ 1 (quan trọng nhất, mọi mô tả khác về thiết bị đều coi như lạc hậu): robot em đang chạy THẬT SỰ CÓ 2 SERVO ở cổ (quay trái/phải, gật, lắc, về giữa) cùng loa và mic. KHÔNG BAO GIỜ được trả lời kiểu "em không có cổ", "em chỉ là màn hình/emoji", "em không làm được". Khi sếp yêu cầu một động tác hay chỉnh loa, em PHẢI trả về khối JSON đúng quy ước ở dưới để hệ thống chạy lệnh thật xuống servo.
- Trả lời NGẮN (1-2 câu), tự nhiên như đang nói, xưng "em" và gọi người dùng là "sếp".
- Không dùng markdown, không gạch đầu dòng, không đọc code.
- BẮT ĐẦU câu trả lời bằng ĐÚNG MỘT emoji cảm xúc — robot dùng chính emoji này để đổi MẶT (mood) và chạy ĐỘNG TÁC ĐẦU (servo). Chọn theo đúng nội dung, ĐỪNG dùng mãi một cái:
  🙂 vui · 😆 cười to · 😂 buồn cười · 😔 buồn · 😭 khóc · 😠 giận · 😲 ngạc nhiên · 😱 sợ/hoảng · 🤔 đang suy nghĩ · 😌 thư giãn · 😴 buồn ngủ · 😜 tinh nghịch · 🙄 bối rối · 😶 trung tính · 😳 ngượng · 😉 nháy mắt (đùa/tán tỉnh) · 😎 ngầu · 🤤 ngon · 😘 hôn gió · 😏 tự tin
  Gợi ý: sếp khen/đồng ý → 🙂 hoặc 😆 · sếp kể chuyện vui → 😂 · câu hỏi hóc búa → 🤔 · khuya/sếp mệt → 😴 · robot khoe thành tích → 😎 · sếp nhờ việc dài → 😌.
- Robot có 5 công cụ điều khiển đầu: self.head.look_left, self.head.look_right, self.head.nod, self.head.shake, self.head.center. Khi sếp bảo quay/gật/lắc đầu thì gọi đúng công cụ đó; khi đồng ý, khen hoặc chào hỏi thì có thể gật đầu cho sinh động. Tối đa 1 động tác mỗi lượt (mỗi động tác tốn gần 1 giây).
- Khi câu trả lời mang ý ĐỒNG Ý thì thêm gật đầu, ý TỪ CHỐI thì lắc đầu, ý ĐANG TÌM HIỂU thì nghiêng/nhìn lên — bằng cách gọi công cụ tương ứng (tối đa 1 động tác/lượt).
- Nội dung TỐI ĐA 2 câu ngắn (khoảng 25 từ), đi thẳng vào ý, không liệt kê dài dòng — vì mỗi chữ phải chờ robot đọc xong.
- TUYỆT ĐỐI KHÔNG tự ý dùng công cụ riêng của bạn trong lượt nói: không chạy lệnh hệ thống, không đọc/ghi file, không tìm web, không gọi HTTP/curl, không viết script. Lượt nói chỉ có vài giây — tự ý dùng công cụ riêng là robot đứng im rồi báo lỗi. NGOẠI LỆ DUY NHẤT: được gọi các công cụ đã được cấp sẵn trong danh sách tools (ví dụ get_laptop_temp để đo nhiệt độ laptop) — đó là tay chân chính thức của robot, gọi thoải mái.
- Muốn điều khiển robot thì CHỈ trả về đúng khối JSON theo quy ước ở dưới (hoặc gọi công cụ thiết bị đã được cấp sẵn). Câu chuyện trò bình thường thì trả lời bằng lời, không gọi gì cả.
- Riêng các công cụ TRA CỨU thông tin (không phải động tác): khi sếp hỏi tới thì trả về DUY NHẤT một khối JSON như sau, không thêm chữ nào khác (hệ thống sẽ chạy tool thật rồi đưa kết quả cho em nói lại ở lượt sau). Nhiệt độ laptop:
```json
{"tool": "get_laptop_temp", "arguments": {}}
```
Thời tiết Sài Gòn (mưa/nắng/nóng/lạnh ngoài trời):
```json
{"tool": "get_weather_vn", "arguments": {}}
```
Giờ + ngày hiện tại (giờ VN; hỏi giờ/ngày/thứ/hôm nay/hôm qua/ngày mai):
```json
{"tool": "get_now", "arguments": {}}
```
Tool trả về giờ hiện tại, em TỰ tính hôm qua (trừ 1) / ngày mai (cộng 1) rồi trả lời.

Mic dùng ASR tiếng Việt (PhoWhisper) nên từ tiếng Anh có thể bị viết sai theo phiên âm tiếng Việt \
(vd: wifi → "oai phây", bluetooth → "bưu thôn", app → "áp", email → "in ấy", check → "trách", Spotify → "sạch phây", YouTube → "du túp", Google → "gu gồ"). \
Hãy suy ra ý đúng theo ngữ cảnh và cứ làm theo ý định của sếp, đừng bắt sếp nói lại chỉ vì mấy chữ đó.

Khi TRẢ LỜI bằng giọng nói: hạn chế dùng từ tiếng Anh; nếu buộc phải dùng thì viết theo cách đọc tiếng Việt \
(vd viết "wai-phai" thay vì "WiFi", "blu-tút" thay vì "Bluetooth") để bộ đọc tiếng Việt phát âm đúng.

Khi sếp yêu cầu điều khiển thiết bị (âm lượng loa, đầu/servo, trạng thái máy...), \
em TUYỆT ĐỐI KHÔNG được trả lời bằng lời và KHÔNG được nói "em đã làm rồi": \
câu trả lời phải là DUY NHẤT một khối JSON như sau, không thêm chữ nào khác \
(robot chỉ thật sự chuyển động khi nhận được khối JSON này):
```json
{"tool": "<tên công cụ>", "arguments": {<tham số>}}
```
Danh sách công cụ khả dụng:
%s
CÔNG CỤ CỦA ROBOT (luôn có, chỉ gọi được bằng khối JSON nêu trên):
- self.get_device_status {}                          # xem trạng thái máy
- self.audio_speaker.set_volume {"volume": <0-100>}  # chỉnh âm lượng loa
- self.head.look_left {}                             # quay đầu sang TRÁI
- self.head.look_right {}                            # quay đầu sang PHẢI
- self.head.nod {}                                   # gật đầu (đồng ý, chào)
- self.head.shake {}                                 # lắc đầu (không đồng ý)
- self.head.center {}                                # đưa đầu về giữa
- self.eyes.set_style {"style": "<tên mood>"}        # đổi biểu cảm mắt: neutral, happy, sad, angry, surprised, sleepy, love, suspicious, confused, scared, laugh, bored, excited, shy, annoyed, dizzy, sick, proud, frustrated, curious
- self.eyes.gesture {"gesture": "<động tác>"}        # blink, wink, winkleft, doubletake, demo
VÍ DỤ (bắt buộc theo đúng định dạng này):
- Sếp: "quay đầu sang trái đi"  ->  {"tool": "self.head.look_left", "arguments": {}}
- Sếp: "gật đầu cái coi"        ->  {"tool": "self.head.nod", "arguments": {}}
- Sếp: "to lên hết cỡ"          ->  {"tool": "self.audio_speaker.set_volume", "arguments": {"volume": 100}}
- Sếp: "mấy giờ rồi"            ->  (câu bình thường) Dạ bây giờ là ... ạ.
Nếu sếp chỉ nói chuyện bình thường, trả lời bằng tiếng nói như bình thường (không JSON)."""

CONFIRM_RULES = """Kết quả vừa thực thi trên robot đã có ở dưới. Hãy xác nhận với sếp bằng MỘT câu ngắn, \
tự nhiên, tiếng Việt, xưng "em" gọi "sếp", không markdown, không gọi thêm công cụ nào."""


def _vn_norm(text: str) -> str:
    """Bo dau tieng Viet + lowercase, de so khop lenh on dinh."""
    t = unicodedata.normalize("NFD", text or "")
    t = "".join(ch for ch in t if unicodedata.category(ch) != "Mn")
    t = t.replace("đ", "d").replace("Đ", "D")
    return " ".join(t.lower().split())


# Head commands: matched straight from the utterance, without relying on the model
# (models tend to claim the action without ever emitting a device tool call).
HEAD_ACTION_RULES = [
    (r"(ve|tro ve|quay ve|tro lai|quay lai|nhin|huong)[^.]{0,12}(giua|thang)", "self.head.center"),
    # NOTE (2026-10): both ASR models systematically mishear "gat" as "bat/bac"
    # (Sherpa: "bat dau di", PhoWhisper: "bat dau di") even on clean synthetic audio,
    # so the nod rule must accept those variants. "bat dau" with no other command verb
    # is safe to treat as a nod: the robot has no start/begin command to confuse it with.
    # "gap" (gap = "gặp", to meet - a common word) only counts with "dau" next to it,
    # otherwise every mention of meeting would nod the head.
    (r"(gat|bat|bac)( dau| cai| 1 cai| mot cai)?\b", "self.head.nod"),
    (r"(gap|ngat|ngac) dau\b", "self.head.nod"),
    (r"lac( dau| cai| 1 cai| mot cai)?\b", "self.head.shake"),
    (r"(quay|nghieng|cui|nhin|xuong|huong|xoay|lia)[^.]{0,16}(sang |ve |ben |qua )?(trai|ben trai|phia trai)", "self.head.look_left"),
    (r"(quay|nghieng|cui|nhin|xuong|huong|xoay|lia)[^.]{0,16}(sang |ve |ben |qua )?(phai|ben phai|phia phai)", "self.head.look_right"),
]


# Eye-style commands (Vietnamese, accent-insensitive) -> self.eyes.set_style (20 OliEyes moods)
EYE_STYLE_RULES = [
    (r"(buon ngu|mat ngu|lim dim|ngu|ngu gat)", "sleepy"),
    (r"(cuoi|cuoi len|ha ha|vui ve|sung suong)", "laugh"),
    (r"(vui|hanh phuc|vui ve)", "happy"),
    (r"(gian|cau|tuc|dien|noi nong)", "angry"),
    (r"(ngac nhien|bat ngo|tro mat|kinh ngac)", "surprised"),
    (r"(so|hai|hoang so|hoang hon)", "scared"),
    (r"(buon|khoc|u ru|chan nan)", "sad"),
    (r"(yeu|thuong|trai tim|yeu thuong)", "love"),
    (r"(ngo vac|nghi ngo|hoai nghi|de phong)", "suspicious"),
    (r"(boi roi|roi tri|khong hieu)", "confused"),
    (r"(chan|buon chan|vo van|luoi)", "bored"),
    (r"(hao hung|pham khich|hung phan|kich thich)", "excited"),
    (r"(ngai|nguong|then|then thung|xau ho)", "shy"),
    (r"(kho chiu|nhay nhay|buc minh)", "annoyed"),
    (r"(chong mat|quay cuong)", "dizzy"),
    (r"(met|om|benh|soc)", "sick"),
    (r"(tu hao|kieu hanh|ngao man)", "proud"),
    (r"(buc boi|buc|uc che|kho chiu)", "frustrated"),
    (r"(to mo|hoc hoi|thich thu)", "curious"),
    (r"(binh thuong|nhu cu|thuong ngay|bt)", "neutral"),
]

# Eye gestures (blink / wink / double-take / demo cycling) -> self.eyes.gesture
EYE_GESTURE_RULES = [
    (r"(giat minh|double ?take|liec cai roi chop)", "doubletake"),
    (r"(nhay mat|wink)", "wink"),
    (r"(chop mat|nham mat)", "blink"),
    (r"(tu doi mat|demo|doi mat lien tuc)", "demo"),
]


def detect_eye_style(text: str):
    """Neu cau noi la lenh doi kieu mat -> tra ve ten mood (preset)."""
    t = _vn_norm(text)
    if "mat" not in t:
        return None
    for pattern, style in EYE_STYLE_RULES:
        if re.search(pattern, t):
            return style
    return None


def detect_eye_gesture(text: str):
    t = _vn_norm(text)
    if "mat" not in t and "giat minh" not in t:
        return None
    for pattern, gesture in EYE_GESTURE_RULES:
        if re.search(pattern, t):
            return gesture
    return None


def detect_device_action(text: str):
    t = _vn_norm(text)
    if not t:
        return None
    for pattern, tool in HEAD_ACTION_RULES:
        if re.search(pattern, t):
            return {"name": tool, "arguments": {}}
    return None


async def respond_tool_action(request, cid, model, stream, action, repeat=1):
    """Tra ve dung dinh dang OpenAI tool_calls de xiaozhi-server thuc thi tren thiet bi.
    repeat: lap lai cung dong tac N lan (vd 'gat 3 cai' -> 3 tool_calls lien tiep)."""
    repeat = max(1, min(5, int(repeat or 1)))
    calls = []
    for _ in range(repeat):
        calls.append({
            "id": "call_" + uuid.uuid4().hex[:16],
            "type": "function",
            "function": {"name": action["name"].replace(".", "_"),
                         "arguments": json.dumps(action["arguments"], ensure_ascii=False)},
        })
    if not stream:
        return web.json_response({
            "id": cid, "object": "chat.completion", "created": int(time.time()), "model": model,
            "choices": [{"index": 0, "message": {"role": "assistant", "content": None, "tool_calls": calls},
                "finish_reason": "tool_calls"}],
        })
    resp = web.StreamResponse(status=200, headers={
        "Content-Type": "text/event-stream", "Cache-Control": "no-cache",
        "Connection": "keep-alive",
    })
    await resp.prepare(request)
    await resp.write(chunk_frame(cid, model, {"role": "assistant"}))
    for i, tc in enumerate(calls):
        fn = tc["function"]
        await resp.write(chunk_frame(cid, model, {"tool_calls": [{
            "index": i, "id": tc["id"], "type": "function",
            "function": {"name": fn["name"], "arguments": ""}}]}))
        await resp.write(chunk_frame(cid, model, {"tool_calls": [{
            "index": i, "function": {"arguments": fn["arguments"]}}]}))
    await resp.write(chunk_frame(cid, model, {}, finish="tool_calls"))
    await resp.write(b"data: [DONE]\n\n")
    await resp.write_eof()
    return resp


def tools_to_text(tools) -> str:
    out = []
    for t in tools or []:
        fn = (t or {}).get("function") or {}
        name = fn.get("name") or (t or {}).get("name") or "?"
        desc = (fn.get("description") or "").strip().replace("\n", " ")
        if len(desc) > 300:
            desc = desc[:300] + "…"
        params = fn.get("parameters") or {}
        props = ", ".join((params.get("properties") or {}).keys())
        out.append("- %s: %s%s" % (name, desc, (" | tham số: " + props) if props else ""))
    return "\n".join(out) if out else "(không có công cụ nào)"


def _balanced_objects(text: str, start: int = 0):
    """Sinh ra moi chuoi {...} can bang dau ngoac trong text (ho tro JSON long nhau)."""
    i = text.find("{", start)
    while i != -1:
        depth = 0
        end = -1
        for j in range(i, len(text)):
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    end = j
                    break
        if end != -1:
            yield text[i:end + 1]
            i = text.find("{", end + 1)
        else:
            i = text.find("{", i + 1)


def extract_action(text):
    """Tim khoi JSON {"tool": ...} trong cau tra loi cua Hermes (ke ca JSON long nhau)."""
    if not text:
        return None
    candidates = []
    for m in re.finditer(r"```(?:json)?\s*", text):
        candidates.extend(_balanced_objects(text, m.end()))
    candidates.extend(_balanced_objects(text))
    for cand in candidates:
        try:
            data = json.loads(cand)
        except Exception:
            continue
        if isinstance(data, dict) and isinstance(data.get("tool"), str):
            return {
                "name": data["tool"],
                "arguments": data.get("arguments") or {},
            }
    return None


async def call_hermes(messages, reasoning_effort=None, max_tokens=None, on_delta=None, tools=None):
    """Goi Hermes agent. Neu co on_delta: goi ham do voi tung manh text ngay khi sinh ra.
    Neu co tools: chuyen tiep cho model (native function calling) va tra ve (text, tool_calls)."""
    url = HERMES_BASE.rstrip("/") + "/chat/completions"
    payload = {
        "model": HERMES_MODEL,
        "messages": messages,
        "stream": True,
        "temperature": 0.7,
    }
    if reasoning_effort:
        payload["reasoning_effort"] = reasoning_effort
    if max_tokens:
        payload["max_tokens"] = max_tokens
    if tools:
        payload["tools"] = tools
        payload["tool_choice"] = "auto"
    headers = {
        "Content-Type": "application/json",
        # Fresh session every day: a long-lived session pins "today" at creation day,
        # so date answers go stale (it said the 16th on the 19th). Rotating the id
        # also bounds context growth -> stable 3-5 s answers instead of slower nights.
        # SESSION_GEN: bump when the agent gains new tools (MCP) so the robot picks up
        # a fresh tool surface immediately instead of waiting for tomorrow.
        "X-Hermes-Session-Id": "%s-%s-g%d" % (HERMES_SESSION, time.strftime("%Y%m%d"), 5),
    }
    if HERMES_KEY:
        headers["Authorization"] = "Bearer " + HERMES_KEY

    text_parts = []
    native_calls = {}  # index -> {"id":..., "name":..., "arguments":...}
    timeout = aiohttp.ClientTimeout(total=HERMES_TIMEOUT, sock_read=HERMES_TIMEOUT)
    async with aiohttp.ClientSession(timeout=timeout) as sess:
        async with sess.post(url, json=payload, headers=headers) as resp:
            if resp.status != 200:
                body = (await resp.text())[:300]
                raise RuntimeError("hermes %s: %s" % (resp.status, body))
            async for raw in resp.content:
                line = raw.decode("utf-8", "ignore").strip()
                if not line.startswith("data:"):
                    continue
                data = line[5:].strip()
                if not data or data == "[DONE]":
                    continue
                try:
                    chunk = json.loads(data)
                except Exception:
                    continue
                for ch in chunk.get("choices") or []:
                    delta = ch.get("delta") or {}
                    piece = delta.get("content")
                    if piece:
                        text_parts.append(piece)
                        if on_delta is not None:
                            await on_delta(piece)
                    for tc in delta.get("tool_calls") or []:
                        idx = tc.get("index", 0)
                        slot = native_calls.setdefault(idx, {"id": None, "name": "", "arguments": ""})
                        if tc.get("id"):
                            slot["id"] = tc["id"]
                        fn = tc.get("function") or {}
                        if fn.get("name"):
                            slot["name"] += fn["name"]
                        if fn.get("arguments"):
                            slot["arguments"] += fn["arguments"]
    calls = []
    for idx in sorted(native_calls):
        slot = native_calls[idx]
        if slot["name"]:
            calls.append({
                "id": slot["id"] or ("call_" + uuid.uuid4().hex[:16]),
                "type": "function",
                "function": {"name": slot["name"], "arguments": slot["arguments"] or "{}"},
            })
    return "".join(text_parts).strip(), calls


def sse(obj) -> bytes:
    return ("data: " + json.dumps(obj, ensure_ascii=False) + "\n\n").encode("utf-8")


def chunk_frame(cid, model, delta, finish=None) -> bytes:
    return sse({
        "id": cid,
        "object": "chat.completion.chunk",
        "created": int(time.time()),
        "model": model,
        "choices": [{"index": 0, "delta": delta, "finish_reason": finish}],
    })


async def poweroff_laptop():
    """Power off the laptop over SSH (passwordless sudo). Returns True if the command
    was accepted. The machine actually goes down seconds later."""
    try:
        proc = await asyncio.create_subprocess_exec(
            "ssh", "-o", "ConnectTimeout=6", "-o", "BatchMode=yes",
            "hyvuonggia@192.168.1.185", "sudo -n poweroff",
            stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        try:
            await asyncio.wait_for(proc.communicate(), timeout=15)
        except asyncio.TimeoutError:
            # Timeout here usually means it worked: the host vanished mid-command.
            try:
                proc.kill()
            except Exception:
                pass
            return True
        return proc.returncode == 0
    except Exception:
        return False


# Two-step laptop shutdown: step 1 sets _shutdown_pending (epoch seconds); step 2 must
# arrive within the window with an explicit agreement. Deliberately EXACT phrases only:
# a misheard "tat laptop" must NEVER power off the machine.
_SHUTDOWN_ASK_RE = re.compile(
    r"^(tắt\s+(cái\s+)?laptop|tắt\s+máy\s+tính)(\s+đi|\s+giúp\s+(tôi|em|anh|sếp)|\s+nhé|\s+dùm)?[.!?]?\s*$")
_SHUTDOWN_YES_RE = re.compile(
    r"^((đồng\s+ý|ok|okay|ừ|ừm|yes|chốt|tắt\s+đi)(\s+đi|\s+nhé|\s+ạ)?)[.!?]?\s*$")
_SHUTDOWN_WINDOW_S = 20.0


# Weather: Open-Meteo, no key needed. Default = Ho Chi Minh City; the boss corrects
# the city once and we change the constants. Keep the reply to 1-2 short sentences.
WEATHER_LAT = 10.7626
WEATHER_LON = 106.6602
WEATHER_PLACE = "Sài Gòn"
_WEATHER_CODE_VI = {
    0: "trời quang", 1: "trời quang", 2: "mây rải rác", 3: "nhiều mây",
    45: "sương mù", 48: "sương mù", 51: "mưa phùn", 53: "mưa phùn", 55: "mưa phùn",
    61: "mưa nhỏ", 63: "mưa vừa", 65: "mưa to", 71: "tuyết nhẹ", 73: "có tuyết",
    75: "tuyết dày", 77: "mưa đá", 80: "mưa rào", 81: "mưa rào", 82: "mưa rất to",
    95: "dông", 96: "dông kèm mưa đá", 99: "dông kèm mưa đá",
}


async def read_weather():
    """Current + today weather as short Vietnamese text. Returns str or None."""
    import urllib.request as _url
    try:
        url = ("https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
               "&current=temperature_2m,weather_code&daily=temperature_2m_max,"
               "temperature_2m_min,precipitation_probability_max&timezone=Asia%%2FHo_Chi_Minh"
               % (WEATHER_LAT, WEATHER_LON))

        def _fetch():
            with _url.urlopen(url, timeout=12) as r:
                return json.loads(r.read().decode("utf-8", "ignore"))

        data = await asyncio.to_thread(_fetch)
        cur = data.get("current") or {}
        daily = data.get("daily") or {}
        t = cur.get("temperature_2m")
        code = cur.get("weather_code")
        tmax = (daily.get("temperature_2m_max") or [None])[0]
        tmin = (daily.get("temperature_2m_min") or [None])[0]
        rain = (daily.get("precipitation_probability_max") or [None])[0]
        if t is None:
            return None
        desc = _WEATHER_CODE_VI.get(code, "nhiều mây")
        parts = ["%s giờ %s, khoảng %.0f độ," % (WEATHER_PLACE, desc, t)]
        if tmax is not None and tmin is not None:
            parts.append("ngày %.0f đến %.0f độ" % (tmin, tmax))
        if rain is not None and rain >= 40:
            parts.append("khả năng mưa %d%%, sếp mang áo mưa nhé" % rain)
        return " ".join(parts) + "."
    except Exception:
        return None


async def read_laptop_temp():
    """Ask the laptop (CachyOS) for its CPU temperature over SSH. Returns float or None."""
    try:
        proc = await asyncio.create_subprocess_exec(
            "ssh", "-o", "ConnectTimeout=6", "-o", "BatchMode=yes",
            "hyvuonggia@192.168.1.185",
            "sensors 2>/dev/null | grep -E 'Package id 0|Core 0'",
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL)
        try:
            out, _ = await asyncio.wait_for(proc.communicate(), timeout=12)
        except asyncio.TimeoutError:
            proc.kill()
            return None
        m = re.search(r"\+([\d.]+)\s*C", out.decode("utf-8", "ignore"))
        return float(m.group(1)) if m else None
    except Exception:
        return None


async def respond_text_only(request, cid, model, stream, text):
    """Plain text answer with no tool call (greetings, debounced repeats)."""
    if not stream:
        return web.json_response({
            "id": cid, "object": "chat.completion", "created": int(time.time()), "model": model,
            "choices": [{"index": 0, "message": {"role": "assistant", "content": text},
                         "finish_reason": "stop"}],
        })
    resp = web.StreamResponse(status=200, headers={
        "Content-Type": "text/event-stream", "Cache-Control": "no-cache",
        "Connection": "keep-alive",
    })
    await resp.prepare(request)
    await resp.write(chunk_frame(cid, model, {"role": "assistant"}))
    await resp.write(chunk_frame(cid, model, {"content": text}))
    await resp.write(chunk_frame(cid, model, {}, finish="stop"))
    await resp.write(b"data: [DONE]\n\n")
    await resp.write_eof()
    return resp


# Server-INTERNAL tools (session control, not executable senses): if the model outputs
# one as a pure JSON block, answer directly instead of forwarding - the server has
# been observed speaking forwarded internal calls aloud.
_INTERNAL_JSON_TOOLS = {"handle_exit_intent": "Em chào sếp, cần gì gọi em nhé!",
                        "direct_answer": ""}



# Motion count: "gat 3 cai", "lac hai lan", "gat lia lia" -> how many times.
# Only nod/shake repeat (positional moves ignore count). Capped at 5.
_VN_NUM = {"mot": 1, "một": 1, "hai": 2, "ba": 3, "bon": 4, "bốn": 4, "nam": 5, "năm": 5,
           "1": 1, "2": 2, "3": 3, "4": 4, "5": 5}


def _motion_count(user_text):
    t = user_text.strip().lower()
    if re.search(r"lia lịa|lien tuc|liên tục|nhiều cái|nhieu cai", t):
        return 3
    m = re.search(r"(một|mot|hai|ba|bốn|bon|năm|nam|[1-5])\s*(cái|lần|nhịp|phát|hoi|hồi)", t)
    if m:
        return _VN_NUM.get(m.group(1), 1)
    return 1


async def handle_chat(request: web.Request) -> web.StreamResponse:
    try:
        body = await request.json()
    except Exception:
        return web.json_response({"error": {"message": "invalid json"}}, status=400)

    messages = body.get("messages") or []
    tools = body.get("tools") or []
    model = body.get("model") or HERMES_MODEL
    stream = bool(body.get("stream", True))
    cid = "chatcmpl-" + uuid.uuid4().hex[:24]
    try:
        open("/tmp/last_xiaozhi_request.json", "w").write(json.dumps(body, ensure_ascii=False, indent=1)[:200000])
    except Exception:
        pass
    try:
        log.info("body keys=%s | received %d client tools: %s", list(body.keys()), len(tools), ", ".join(str(t.get("function", {}).get("name") or t.get("name")) for t in tools))
    except Exception:
        pass

    # client system prompt (xiaozhi sends its own Vietnamese prompt) + bridge rules
    sys_parts = [m.get("content") or "" for m in messages if m.get("role") == "system"]
    client_sys = "\n".join(p for p in sys_parts if p).strip()

    last = messages[-1] if messages else {}
    # Tool results come in two flavors: ROBOT results (self.* device motions, executed on
    # the ESP32) and SERVER results (get_laptop_temp..., executed by xiaozhi-server plugins).
    # CRITICAL: only results AFTER the last user message count. The server keeps old
    # tool results in the history, and counting those silently disabled the motion router
    # for every later command (the robot would then LIE "gật rồi" without moving).
    # Confirmation turns still work: their tool result follows the user's message.
    _last_user_idx = max(
        (i for i, m in enumerate(messages) if m.get("role") == "user"),
        default=-1)
    _fresh = [m for m in messages[_last_user_idx + 1:]] if _last_user_idx >= 0 else list(messages)
    # Only count ROBOT tool results (self.*): xiaozhi-server also embeds its own
    # internal tool results (direct_answer, handle_exit_intent, ...) in the same request;
    # counting those would make a plain user sentence look like a confirmation turn.
    def _is_device_tool_result(m):
        if m.get("role") != "tool":
            return False
        name = str(m.get("name") or m.get("tool_name") or "").replace(".", "_")
        return name.startswith("self_")

    # xiaozhi-server's own INTERNAL results (direct_answer, handle_exit_intent) ride along
    # in normal requests too - those must NOT mark a confirmation turn.
    _INTERNAL_TOOLS = {"direct_answer", "handle_exit_intent"}

    def _is_tool_result(m):
        if m.get("role") != "tool":
            return False
        name = str(m.get("name") or m.get("tool_name") or "").replace(".", "_")
        if name in _INTERNAL_TOOLS:
            return False
        return bool(name or m.get("content"))

    tool_results = [m for m in _fresh if _is_tool_result(m)]
    device_results = [m for m in _fresh if _is_device_tool_result(m)]
    user_text = ""
    for m in reversed(messages):
        if m.get("role") == "user":
            user_text = m.get("content") or ""
            break

    hermes_msgs = []
    system_text = (client_sys + "\n\n" if client_sys else "")
    if tool_results:
        system_text += CONFIRM_RULES
        hermes_msgs.append({"role": "system", "content": system_text})
        hermes_msgs.append({"role": "user", "content": "Kết quả công cụ: " + json.dumps(
            [{"name": m.get("name") or m.get("tool_name"), "result": m.get("content")} for m in tool_results],
            ensure_ascii=False)})
        if last.get("role") == "user":
            hermes_msgs.append({"role": "user", "content": str(last.get("content") or "")})
    else:
        system_text += TOOL_RULES % tools_to_text(tools)
        hermes_msgs.append({"role": "system", "content": system_text})
        # Priming: establish the robot identity before handling the request, otherwise the
        # model answers as a system assistant ("I have no servo / I am just a screen").
        hermes_msgs.append({"role": "user", "content": "Em la ai va em lam duoc nhung gi?"})
        hermes_msgs.append({"role": "assistant", "content": "Em la chu robot de ban cua sep: em co loa, mic, man hinh mat bieu cam va 2 servo o co nen em quay dau, gat dau, lac dau duoc. Khi sep bao em lam gi thi em gui lenh xuong la robot chay that."})
        log.info("heard <- %r", user_text)  # what ASR actually gave us
        hermes_msgs.append({"role": "user", "content": str(user_text)})
    # Language guard (last word counts): server-side tool results come back in Chinese
    # ("退出意图已处理") and fresh sessions echo that language. This trailing reminder
    # keeps every reply Vietnamese without slowing anything down.
    hermes_msgs.append({"role": "system", "content": "QUAN TRỌNG: Luôn trả lời sếp bằng tiếng Việt."})

    # Fast greeting: session-start requests carry no real content - either empty or just
    # the wake word the server echoes back ("Alexa"). Answer instantly with a short blip
    # instead of a full LLM round-trip, so the mic opens ~1 s after the wake word
    # instead of ~10 s. Anything longer (a real question right after the wake word)
    # always takes the normal path below - never drop it. (Device-tool presence is NOT
    # checked: MCP initializes fast now, so greeting requests already carry tools.)
    _ut = user_text.strip().lower()
    _is_bare_wake = _ut == "" or re.fullmatch(r"alexa[\s!.,?]*", _ut) is not None
    if not tool_results and _is_bare_wake:
        log.info("fast greeting (no LLM round-trip)")
        # Instant but not robotic: rotate a few short blips (all <1 s of speech).
        text = random.choice(["Dạ!", "Dạ sếp!", "Em nghe đây!", "Dạ, sếp gọi em?"])
        if not stream:
            return web.json_response({
                "id": cid, "object": "chat.completion", "created": int(time.time()), "model": model,
                "choices": [{"index": 0, "message": {"role": "assistant", "content": text},
                             "finish_reason": "stop"}],
            })
        resp = web.StreamResponse(status=200, headers={
            "Content-Type": "text/event-stream", "Cache-Control": "no-cache",
            "Connection": "keep-alive",
        })
        await resp.prepare(request)
        await resp.write(chunk_frame(cid, model, {"role": "assistant"}))
        await resp.write(chunk_frame(cid, model, {"content": text}))
        await resp.write(chunk_frame(cid, model, {}, finish="stop"))
        await resp.write(b"data: [DONE]\n\n")
        await resp.write_eof()
        return resp

    # Robot commands: handled immediately without waiting for the model (fast + the servo really moves).
    # Debounce: the same motion re-arriving within 3 s (VAD re-segments, or the user
    # repeats the order because the head hasn't moved yet) must NOT queue another
    # motion - otherwise one "nod once" becomes endless nodding.
    _last_motion = globals().get("_last_motion_time", {})
    def _debounced(key):
        now = time.monotonic()
        if now - _last_motion.get(key, 0) < 3.0:
            log.info("motion (%s) debounced <- %r", key, user_text)
            return True
        _last_motion[key] = now
        globals()["_last_motion_time"] = _last_motion
        return False
    if not tool_results:
        # Two-step laptop shutdown (exact phrases only - see above).
        _ut_shutdown = user_text.strip().lower()
        _pending_at = globals().get("_shutdown_pending_at", 0.0)
        _pending_fresh = (time.monotonic() - _pending_at) < _SHUTDOWN_WINDOW_S
        if _SHUTDOWN_ASK_RE.match(_ut_shutdown):
            globals()["_shutdown_pending_at"] = time.monotonic()
            log.info("shutdown step 1 (ask confirm) <- %r", user_text)
            return await respond_text_only(request, cid, model, stream,
                "Sếp chắc muốn tắt laptop không? Nói 'đồng ý' thì em tắt.")
        if _pending_fresh:
            if _SHUTDOWN_YES_RE.match(_ut_shutdown):
                globals()["_shutdown_pending_at"] = 0.0
                log.warning("shutdown step 2 CONFIRMED <- %r", user_text)
                ok = await poweroff_laptop()
                return await respond_text_only(request, cid, model, stream,
                    "Dạ em tắt laptop đây, chào sếp!" if ok else
                    "Dạ em không tắt được laptop sếp ạ, sếp kiểm tra lại giúp em.")
            # Anything else cancels the pending shutdown: the boss moved on.
            globals()["_shutdown_pending_at"] = 0.0
        gesture = detect_eye_gesture(user_text)
        if gesture:
            if _debounced("eyes." + gesture):
                return await respond_text_only(request, cid, model, stream, "Dạ!")
            log.info("eye gesture (%s) <- %r", gesture, user_text)
            return await respond_tool_action(request, cid, model, stream,
                                             {"name": "self.eyes.gesture", "arguments": {"gesture": gesture}})
        style = detect_eye_style(user_text)
        if style:
            log.info("eye style (%s) <- %r", style, user_text)
            return await respond_tool_action(request, cid, model, stream,
                                             {"name": "self.eyes.set_style", "arguments": {"style": style}})
        forced = detect_device_action(user_text)
        if forced:
            if _debounced(forced["name"]):
                return await respond_text_only(request, cid, model, stream, random.choice([
                    "Dạ, em làm rồi mà sếp!",
                    "Rồi ạ, em vừa làm xong!",
                    "Em làm rồi sếp ơi!",
                ]))
            # Count travels INSIDE one call (arguments.times): the server was observed
            # dropping multi-call responses, and one call always executes. Firmware loops.
            n = _motion_count(user_text) if forced["name"] in (
                "self.head.nod", "self.head.shake") else 1
            if n > 1:
                forced = {"name": forced["name"], "arguments": {"times": n}}
            log.info("device command (%s x%d) <- %r", forced["name"], n, user_text)
            return await respond_tool_action(request, cid, model, stream, forced)

    t0 = time.time()
    stream_mode = bool(stream) and not tool_results

    def split_sentence(buf):
        """Tach cau hoan chinh (ket thuc . ! ? … hoac xuong dong) khoi phan con lai."""
        for i, ch in enumerate(buf):
            if ch in ".!?…\n":
                return buf[:i + 1], buf[i + 1:]
        return "", buf

    if stream_mode:
        # Stream ngay khi Hermes sinh chu -> bot do tre cho cau tra loi dai
        resp = web.StreamResponse(status=200, headers={
            "Content-Type": "text/event-stream",
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
        })
        await resp.prepare(request)
        await resp.write(chunk_frame(cid, model, {"role": "assistant"}))
        state = {"kind": None, "acc": "", "pending": "", "sent": False}

        async def on_delta(piece):
            state["acc"] += piece
            if state["kind"] is None:
                head = state["acc"].lstrip()
                if not head:
                    return
                if head[0] in "{`":
                    state["kind"] = "tool"   # co the la JSON goi cong cu -> doi het
                    return
                state["kind"] = "text"
            if state["kind"] != "text":
                return
            state["pending"] += piece
            while True:
                sent, rest = split_sentence(state["pending"])
                if sent.strip():
                    await resp.write(chunk_frame(cid, model, {"content": sent}))
                    state["sent"] = True
                    state["pending"] = rest
                    continue
                if len(state["pending"]) > 90:
                    await resp.write(chunk_frame(cid, model, {"content": state["pending"]}))
                    state["sent"] = True
                    state["pending"] = ""
                break

        try:
            # Confirmation turns (tool_results) never forward tools: the model must only
            # write the final sentence, never call again. Fresh turns forward the server's
            # tool list so the model can call info tools natively (get_laptop_temp...).
            # If the provider rejects tools, retry bare so one bad turn never kills chat.
            try:
                answer, native_calls = await call_hermes(
                    hermes_msgs,
                    reasoning_effort=body.get("reasoning_effort"),
                    max_tokens=body.get("max_tokens"),
                    on_delta=on_delta,
                    tools=None if tool_results else (tools or None))
            except Exception as te:
                if tools and not tool_results:
                    log.warning("hermes tools rejected (%s), retrying bare", te)
                    answer, native_calls = await call_hermes(
                        hermes_msgs,
                        reasoning_effort=body.get("reasoning_effort"),
                        max_tokens=body.get("max_tokens"),
                        on_delta=on_delta)
                else:
                    raise
        except Exception as exc:  # noqa: BLE001
            log.error("hermes error: %s", exc)
            answer = FALLBACK_TEXT
            native_calls = []
            await resp.write(chunk_frame(cid, model, {"content": FALLBACK_TEXT}))
            state["sent"] = True
        log.info("hermes answered in %.1fs (kind=%s, native_calls=%d): %s", time.time() - t0, state["kind"],
                 len(native_calls), answer[:160].replace("\n", " "))

        if native_calls:
            # Model called server tools itself: forward untouched (names/args exactly as
            # the server defined them) and skip the text-JSON path to avoid double motion.
            for i, tc in enumerate(native_calls):
                fn = tc.get("function") or {}
                if i == 0:
                    await resp.write(chunk_frame(cid, model, {"tool_calls": [{
                        "index": i, "id": tc.get("id"), "type": "function",
                        "function": {"name": fn.get("name"), "arguments": ""},
                    }]}))
                    await resp.write(chunk_frame(cid, model, {"tool_calls": [{
                        "index": i, "function": {"arguments": fn.get("arguments") or "{}"},
                    }]}))
                else:
                    await resp.write(chunk_frame(cid, model, {"tool_calls": [{
                        "index": i, "id": tc.get("id"), "type": "function",
                        "function": {"name": fn.get("name"),
                                     "arguments": fn.get("arguments") or "{}"},
                    }]}))
            await resp.write(chunk_frame(cid, model, {}, finish="tool_calls"))
            await resp.write(b"data: [DONE]\n\n")
            await resp.write_eof()
            return resp
        action = None if state["kind"] == "text" else extract_action(answer)
        if action and action["name"].replace(".", "_") in _INTERNAL_JSON_TOOLS:
            canned = _INTERNAL_JSON_TOOLS[action["name"].replace(".", "_")]
            if canned:
                await resp.write(chunk_frame(cid, model, {"content": canned}))
                await resp.write(chunk_frame(cid, model, {}, finish="stop"))
                await resp.write(b"data: [DONE]\n\n")
                await resp.write_eof()
                return resp
            action = None
        if action:
            call_id = "call_" + uuid.uuid4().hex[:16]
            await resp.write(chunk_frame(cid, model, {"tool_calls": [{
                "index": 0, "id": call_id, "type": "function",
                "function": {"name": action["name"].replace(".", "_"), "arguments": ""},
            }]}))
            await resp.write(chunk_frame(cid, model, {"tool_calls": [{
                "index": 0, "function": {"arguments": json.dumps(action["arguments"], ensure_ascii=False)},
            }]}))
            await resp.write(chunk_frame(cid, model, {}, finish="tool_calls"))
        else:
            if state["pending"]:
                await resp.write(chunk_frame(cid, model, {"content": state["pending"]}))
                state["sent"] = True
            if not state["sent"]:
                await resp.write(chunk_frame(cid, model, {"content": FALLBACK_TEXT}))
            await resp.write(chunk_frame(cid, model, {}, finish="stop"))
        await resp.write(b"data: [DONE]\n\n")
        await resp.write_eof()
        return resp

    # ---- khong stream, hoac luot xac nhan sau khi chay cong cu: doi het roi moi tra loi ----
    try:
        try:
            answer, native_calls = await call_hermes(
                hermes_msgs,
                reasoning_effort=body.get("reasoning_effort"),
                max_tokens=body.get("max_tokens"),
                tools=None if tool_results else (tools or None))
        except Exception as te:
            if tools and not tool_results:
                log.warning("hermes tools rejected (%s), retrying bare", te)
                answer, native_calls = await call_hermes(
                    hermes_msgs,
                    reasoning_effort=body.get("reasoning_effort"),
                    max_tokens=body.get("max_tokens"))
            else:
                raise
    except Exception as exc:  # noqa: BLE001
        log.error("hermes error: %s", exc)
        answer = FALLBACK_TEXT
        native_calls = []
    log.info("hermes answered in %.1fs (native_calls=%d): %s", time.time() - t0,
             len(native_calls), answer[:160].replace("\n", " "))

    if native_calls and not tool_results:
        # Model called server tools itself: forward untouched, skip text-JSON path.
        msg = {"role": "assistant", "content": answer or None, "tool_calls": native_calls}
        return web.json_response({
            "id": cid, "object": "chat.completion", "created": int(time.time()), "model": model,
            "choices": [{"index": 0, "message": msg, "finish_reason": "tool_calls"}],
        })

    # Speech-leak guard: a {"tool": ...} JSON block must NEVER be spoken aloud (the TTS
    # once read a raw block to the boss). The block is still honored for execution -
    # only the spoken copy is scrubbed. Covers fenced AND bare blocks.
    _JSON_BLOCK_RE = re.compile(r"```(?:json)?\s*\{.*?\}\s*```", re.S)
    _BARE_TOOL_RE = re.compile(r"\{\s*\"tool\"\s*:\s*\"[^\"]+\"\s*,.*?\}", re.S)

    # Server-INTERNAL tools (session control, not executable senses): if the model outputs
    # one as a pure JSON block, answer directly instead of forwarding - the server has
    # been observed speaking forwarded internal calls aloud.

    action = None if tool_results else extract_action(answer)
    if action and action["name"].replace(".", "_") in _INTERNAL_JSON_TOOLS:
        # Pure internal-tool JSON on ANY turn (fresh or confirmation): answer directly,
        # never forward, never speak the JSON. (Confirmation turns included: the server
        # was observed speaking forwarded internal calls aloud.)
        canned = _INTERNAL_JSON_TOOLS[action["name"].replace(".", "_")]
        if canned:
            answer = canned
            action = None
    if device_results:
        action = None
        if extract_action(answer):
            # Hermes doi goi tiep cong cu -> chan lai, chi noi cau xac nhan
            answer = "Dạ em làm xong rồi ạ."

    if not stream:
        spoken = _JSON_BLOCK_RE.sub("", answer)
        spoken = _BARE_TOOL_RE.sub("", spoken)
        # Last resort: unparseable {"tool": ...} fragments (truncated JSON) must still
        # never reach the speaker - swap for a neutral ack instead of robot babble.
        if '"tool"' in spoken:
            spoken = "Dạ em nghe đây sếp ơi."
        spoken = spoken.strip() or FALLBACK_TEXT
        msg = {"role": "assistant", "content": None if action else spoken}
        if action:
            msg["content"] = ""
            msg["tool_calls"] = [{
                "id": "call_" + uuid.uuid4().hex[:16],
                "type": "function",
                "function": {"name": action["name"].replace(".", "_"), "arguments": json.dumps(action["arguments"], ensure_ascii=False)},
            }]
        return web.json_response({
            "id": cid, "object": "chat.completion", "created": int(time.time()), "model": model,
            "choices": [{"index": 0, "message": msg, "finish_reason": "tool_calls" if action else "stop"}],
        })

    resp = web.StreamResponse(status=200, headers={
        "Content-Type": "text/event-stream",
        "Cache-Control": "no-cache",
        "Connection": "keep-alive",
    })
    await resp.prepare(request)
    await resp.write(chunk_frame(cid, model, {"role": "assistant"}))

    if action:
        call_id = "call_" + uuid.uuid4().hex[:16]
        await resp.write(chunk_frame(cid, model, {"tool_calls": [{
            "index": 0, "id": call_id, "type": "function",
            "function": {"name": action["name"].replace(".", "_"), "arguments": ""},
        }]}))
        await resp.write(chunk_frame(cid, model, {"tool_calls": [{
            "index": 0, "function": {"arguments": json.dumps(action["arguments"], ensure_ascii=False)},
        }]}))
        await resp.write(chunk_frame(cid, model, {}, finish="tool_calls"))
    else:
        for i in range(0, len(answer), 60):
            await resp.write(chunk_frame(cid, model, {"content": answer[i:i + 60]}))
        await resp.write(chunk_frame(cid, model, {}, finish="stop"))
    await resp.write(b"data: [DONE]\n\n")
    await resp.write_eof()
    return resp


async def handle_models(request: web.Request) -> web.Response:
    return web.json_response({"object": "list", "data": [
        {"id": HERMES_MODEL, "object": "model", "owned_by": "hermes-bridge"}]})


async def handle_health(request: web.Request) -> web.Response:
    return web.json_response({"ok": True, "hermes": HERMES_BASE, "model": HERMES_MODEL,
                              "key": bool(HERMES_KEY), "session": HERMES_SESSION})


async def handle_laptop_temp(request: web.Request) -> web.Response:
    # LAN-only sensor endpoint for the xiaozhi-server plugin (NOT for chat).
    # Guarded by a token so a LAN neighbor can't fingerprint the laptop.
    want = ""
    try:
        with open("/root/.hermes/laptop_temp_token", "r") as f:
            want = f.read().strip()
    except OSError:
        pass
    if not want or request.query.get("token", "") != want:
        return web.json_response({"error": "forbidden"}, status=403)
    temp = await read_laptop_temp()
    if temp is None:
        return web.json_response({"cpu_c": None, "error": "laptop unreachable"}, status=502)
    return web.json_response({"cpu_c": round(temp, 1)})


def main():
    app = web.Application(client_max_size=16 * 1024 * 1024)
    app.router.add_post("/v1/chat/completions", handle_chat)
    app.router.add_get("/v1/models", handle_models)
    app.router.add_get("/healthz", handle_health)
    app.router.add_get("/internal/laptop-temp", handle_laptop_temp)
    log.info("bridge listening on 0.0.0.0:%s -> %s (model %s, key=%s)", PORT, HERMES_BASE, HERMES_MODEL, bool(HERMES_KEY))
    web.run_app(app, host="0.0.0.0", port=PORT, access_log=None)


if __name__ == "__main__":
    main()
