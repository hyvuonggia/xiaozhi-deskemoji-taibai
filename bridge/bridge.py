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
- BẮT ĐẦU câu trả lời bằng ĐÚNG MỘT emoji cảm xúc để robot hiện đúng mặt: 😊 (vui), 😢 (buồn), 😠 (giận), 😮 (ngạc nhiên), 🙂 (bình thường). Sau đó mới tới nội dung.
- Robot có 5 công cụ điều khiển đầu: self.head.look_left, self.head.look_right, self.head.nod, self.head.shake, self.head.center. Khi sếp bảo quay/gật/lắc đầu thì gọi đúng công cụ đó; khi đồng ý, khen hoặc chào hỏi thì có thể gật đầu cho sinh động. Tối đa 1 động tác mỗi lượt (mỗi động tác tốn gần 1 giây).
- Nội dung TỐI ĐA 2 câu ngắn (khoảng 25 từ), đi thẳng vào ý, không liệt kê dài dòng — vì mỗi chữ phải chờ robot đọc xong.
- TUYỆT ĐỐI KHÔNG dùng công cụ riêng của bạn trong lượt nói: không chạy lệnh hệ thống, không đọc/ghi file, không tìm web, không gọi HTTP/curl, không viết script. Lượt nói chỉ có vài giây — dùng công cụ riêng là robot đứng im rồi báo lỗi.
- Muốn điều khiển robot thì CHỈ trả về đúng khối JSON theo quy ước ở dưới (hoặc gọi công cụ thiết bị đã được cấp sẵn). Câu chuyện trò bình thường thì trả lời bằng lời, không gọi gì cả.

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
- self.eyes.set_style {"style": "<tên kiểu>"}        # đổi kiểu mắt: normal, happy, glee, sad, angry, surprised, sleepy, squint, focused, skeptic, unimpressed, suspicious, furious, scared, awe, annoyed, worried, frustrated…
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


# Lenh dieu khien dau: nhan dien truc tiep o cau noi, KHONG phu thuoc vao model
# (model hay tuong minh "da lam roi" nhung khong gui lenh xuong thiet bi).
HEAD_ACTION_RULES = [
    (r"(ve|tro ve|quay ve|tro lai|quay lai|nhin|huong)[^.]{0,12}(giua|thang)", "self.head.center"),
    (r"gat( dau| cai| 1 cai| mot cai)?\b", "self.head.nod"),
    (r"lac( dau| cai| 1 cai| mot cai)?\b", "self.head.shake"),
    (r"(quay|nghieng|cui|nhin|xuong|huong|xoay|lia)[^.]{0,16}(sang |ve |ben |qua )?(trai|ben trai|phia trai)", "self.head.look_left"),
    (r"(quay|nghieng|cui|nhin|xuong|huong|xoay|lia)[^.]{0,16}(sang |ve |ben |qua )?(phai|ben phai|phia phai)", "self.head.look_right"),
]


# Lenh doi kieu mat (tieng Viet, khong dau) -> self.eyes.set_style
EYE_STYLE_RULES = [
    (r"(buon ngu|mat ngu|lim dim|ngu)", "sleepy"),
    (r"(cuoi|vui|hanh phuc|sung suong|cuoi len)", "glee"),
    (r"(gian|cau|tuc|dien)", "angry"),
    (r"(ngac nhien|bat ngo|tro mat)", "surprised"),
    (r"(so|hai hung|hoang)", "scared"),
    (r"(buon|khoc|u ru)", "sad"),
    (r"(kho hieu|hoai nghi|nghi ngo)", "skeptic"),
    (r"(coi thuong|khinh|khong hai long)", "unimpressed"),
    (r"(tap trung|nghiem tuc|ngau|chat)", "focused"),
    (r"(binh thuong|nhu cu|thuong)", "normal"),
]


def detect_eye_style(text: str):
    """Neu cau noi la lenh doi kieu mat -> tra ve kieu (ten preset)."""
    t = _vn_norm(text)
    if not t or ("mat" not in t and "mat" not in t):
        return None
    if "mat" not in t:
        return None
    for pattern, style in EYE_STYLE_RULES:
        if re.search(pattern, t):
            return style
    return None


def detect_device_action(text: str):
    t = _vn_norm(text)
    if not t:
        return None
    for pattern, tool in HEAD_ACTION_RULES:
        if re.search(pattern, t):
            return {"name": tool, "arguments": {}}
    return None


async def respond_tool_action(request, cid, model, stream, action):
    """Tra ve dung dinh dang OpenAI tool_calls de xiaozhi-server thuc thi tren thiet bi."""
    call_id = "call_" + uuid.uuid4().hex[:16]
    if not stream:
        return web.json_response({
            "id": cid, "object": "chat.completion", "created": int(time.time()), "model": model,
            "choices": [{"index": 0, "message": {"role": "assistant", "content": None, "tool_calls": [
                {"id": call_id, "type": "function",
                 "function": {"name": action["name"].replace(".", "_"), "arguments": json.dumps(action["arguments"], ensure_ascii=False)}}]},
                "finish_reason": "tool_calls"}],
        })
    resp = web.StreamResponse(status=200, headers={
        "Content-Type": "text/event-stream", "Cache-Control": "no-cache", "Connection": "keep-alive",
    })
    await resp.prepare(request)
    await resp.write(chunk_frame(cid, model, {"role": "assistant"}))
    await resp.write(chunk_frame(cid, model, {"tool_calls": [{
        "index": 0, "id": call_id, "type": "function",
        "function": {"name": action["name"].replace(".", "_"), "arguments": ""}}]}))
    await resp.write(chunk_frame(cid, model, {"tool_calls": [{
        "index": 0, "function": {"arguments": json.dumps(action["arguments"], ensure_ascii=False)}}]}))
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


async def call_hermes(messages, reasoning_effort=None, max_tokens=None, on_delta=None):
    """Goi Hermes agent. Neu co on_delta: goi ham do voi tung manh text ngay khi sinh ra."""
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
    headers = {
        "Content-Type": "application/json",
        "X-Hermes-Session-Id": HERMES_SESSION,
    }
    if HERMES_KEY:
        headers["Authorization"] = "Bearer " + HERMES_KEY

    text_parts = []
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
    return "".join(text_parts).strip()


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
        log.info("body keys=%s | nhan %d tool tu client: %s", list(body.keys()), len(tools), ", ".join(str(t.get("function", {}).get("name") or t.get("name")) for t in tools))
    except Exception:
        pass

    # system prompt cua client (xiaozhi gui prompt tieng Viet) + luat cua bridge
    sys_parts = [m.get("content") or "" for m in messages if m.get("role") == "system"]
    client_sys = "\n".join(p for p in sys_parts if p).strip()

    last = messages[-1] if messages else {}
    # CHI tinh ket qua cong cu cua ROBOT (self.*): xiaozhi-server cung chen ket qua
    # cong cu noi bo cua no (direct_answer, handle_exit_intent...) vao cung request,
    # neu tinh ca chung thi cau noi cua sep bi hieu nham thanh luot xac nhan.
    def _is_device_tool_result(m):
        if m.get("role") != "tool":
            return False
        name = str(m.get("name") or m.get("tool_name") or "").replace(".", "_")
        return name.startswith("self_")

    tool_results = [m for m in messages if _is_device_tool_result(m)]
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
        # Moi (priming): xac lap danh tinh robot truoc khi xu ly cau cua sep,
        # neu khong mo hinh se tra loi voi tu cach tro ly he thong ("em khong co servo").
        hermes_msgs.append({"role": "user", "content": "Em la ai va em lam duoc nhung gi?"})
        hermes_msgs.append({"role": "assistant", "content": "Em la chu robot de ban cua sep: em co loa, mic, man hinh mat bieu cam va 2 servo o co nen em quay dau, gat dau, lac dau duoc. Khi sep bao em lam gi thi em gui lenh xuong la robot chay that."})
        hermes_msgs.append({"role": "user", "content": str(user_text)})

    # Lenh dieu khien robot: xu ly ngay, khong cho model (nhanh + chac chan servo chay)
    if not tool_results:
        style = detect_eye_style(user_text)
        if style:
            log.info("lenh doi kieu mat (%s) <- %r", style, user_text)
            return await respond_tool_action(request, cid, model, stream,
                                             {"name": "self.eyes.set_style", "arguments": {"style": style}})
        forced = detect_device_action(user_text)
        if forced:
            log.info("lenh thiet bi (%s) <- %r", forced["name"], user_text)
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
            answer = await call_hermes(hermes_msgs,
                                       reasoning_effort=body.get("reasoning_effort"),
                                       max_tokens=body.get("max_tokens"),
                                       on_delta=on_delta)
        except Exception as exc:  # noqa: BLE001
            log.error("hermes loi: %s", exc)
            answer = FALLBACK_TEXT
            await resp.write(chunk_frame(cid, model, {"content": FALLBACK_TEXT}))
            state["sent"] = True
        log.info("hermes tra loi trong %.1fs (kind=%s): %s", time.time() - t0, state["kind"],
                 answer[:160].replace("\n", " "))

        action = None if state["kind"] == "text" else extract_action(answer)
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
        answer = await call_hermes(hermes_msgs,
                                   reasoning_effort=body.get("reasoning_effort"),
                                   max_tokens=body.get("max_tokens"))
    except Exception as exc:  # noqa: BLE001
        log.error("hermes loi: %s", exc)
        answer = FALLBACK_TEXT
    log.info("hermes tra loi trong %.1fs: %s", time.time() - t0, answer[:160].replace("\n", " "))

    action = None if tool_results else extract_action(answer)
    if tool_results:
        action = None
        if extract_action(answer):
            # Hermes doi goi tiep cong cu -> chan lai, chi noi cau xac nhan
            answer = "Dạ em làm xong rồi ạ."

    if not stream:
        msg = {"role": "assistant", "content": None if action else answer}
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


def main():
    app = web.Application(client_max_size=16 * 1024 * 1024)
    app.router.add_post("/v1/chat/completions", handle_chat)
    app.router.add_get("/v1/models", handle_models)
    app.router.add_get("/healthz", handle_health)
    log.info("bridge nghe tren 0.0.0.0:%s -> %s (model %s, key=%s)", PORT, HERMES_BASE, HERMES_MODEL, bool(HERMES_KEY))
    web.run_app(app, host="0.0.0.0", port=PORT, access_log=None)


if __name__ == "__main__":
    main()
