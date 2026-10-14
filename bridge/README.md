# Hermes ⇄ Xiaozhi bridge

Puts **Hermes Agent** behind the voice robot: the device talks through xiaozhi-server,
xiaozhi-server thinks the bridge is an OpenAI-compatible LLM, and the bridge forwards
every turn to a Hermes agent session — so the robot has the same brain, memory and
personality as the Hermes chat, and can still execute the device's own MCP tools.

```
robot (ESP32-S3 Desk-Emoji)
   │  opus audio / json            ▲  text + tool_calls (self.*)
   ▼                               │
xiaozhi-server (WS :8000, OTA :8003)
   │  OpenAI /v1/chat/completions  ▲  SSE stream (+ tool_calls)
   ▼                               │
bridge.py (:8643)  ──►  Hermes api_server (:8642, model `hermes-agent`, session `robot-deskemoji`)
```

## What the bridge adds on top of a plain proxy

1. **Sentence streaming** — Hermes' answer is relayed sentence by sentence, so TTS starts
   as soon as the first sentence exists (cuts the "robot thinks for ages" gap).
2. **Deterministic voice-command router** — Vietnamese head/neck commands are matched with
   accent-insensitive regexes and turned into `tool_calls` immediately, without asking the
   model. Reason: language models reliably *claim* they moved ("em đã cúi đầu rồi") without
   ever emitting a tool call, and prompt-only fixes do not hold.
   Supported: look left / right, nod, shake, center (see `HEAD_ACTION_RULES`).
3. **Device tool results only** — xiaozhi-server mixes its own internal tool results
   (`direct_answer`, `handle_exit_intent`, …) into the same request; the bridge filters them
   out, otherwise a normal sentence is mistaken for a tool-confirmation turn.
4. **Timeout guard** — `HERMES_TIMEOUT` (default 25 s) then a short fallback line, so the
   robot always says something instead of staying silent until the server times out.

## Setup

```bash
# 1. copy the bridge next to the Hermes install
mkdir -p /root/hermes-robot-bridge && cp bridge.py /root/hermes-robot-bridge/

# 2. install the unit (edit HERMES_BASE / HERMES_SESSION for your network)
cp hermes-robot-bridge.service /etc/systemd/system/
systemctl daemon-reload && systemctl enable --now hermes-robot-bridge

# 3. point xiaozhi-server at the bridge (data/.config.yaml)
#    LLM:
#      HermesAgent:
#        type: openai
#        base_url: http://<hermes-host>:8643/v1
#        model_name: hermes-agent
#        api_key: <any string>          # not used, must be non-empty
#        reasoning_effort: low          # forwarded to Hermes to cut latency
#    selected_module:
#      LLM: HermesAgent
```

Hermes side:

* the API key is read from `HERMES_KEY` or `API_SERVER_KEY=` in `~/.hermes/.env`
  (never stored in this repo);
* recommended: `hermes config set platform_toolsets.api_server '[]'` — an empty toolset list
  for the `api_server` platform (the robot session) stops the agent from reaching for
  `terminal` / `search_files` / `tool_search` mid-voice-turn, which used to burn 25–30 s
  and make the server time out. Other platforms (telegram, …) are unaffected;
* a fresh `HERMES_SESSION` per robot keeps the context small (~8k tokens → ~3 s/turn).
  A session that grows to 60k+ tokens makes a single turn take >25 s.

## Device tool naming (gotcha)

Firmware advertises tools with dots (`self.head.look_left`) but **xiaozhi-server normalises
them to underscores** — its function list shows `self_head_look_left`. Tool calls must use
the underscore form, otherwise the server answers *"工具 … 不存在"* (tool does not exist).

## Testing without the robot

`tools/fake_device.py` speaks the device websocket protocol (hello → mcp initialize/tools/list
→ listen:start → real-time opus frames → listen:stop) and measures the time from end of speech
to the first TTS audio frame.

```bash
# on the xiaozhi-server host
docker cp tools/fake_device.py xiaozhi-server:/tmp/
docker exec -w /opt/xiaozhi-esp32-server xiaozhi-server \
    python /tmp/fake_device.py /opt/xiaozhi-esp32-server/data/tmp/question.wav 2
```

Notes: opus frames must be sent in **real time** (60 ms each) — sending them faster makes ASR
see an almost empty file. Measured on this setup: ASR 0.03–0.1 s, Hermes turn 2.4–3.7 s,
TTS 0.2 s (≈3–5 s per reply), direct command routing ≈0.05 s.

## Known issue

xiaozhi-server registers the device's MCP tool list late (right after the first audio of a
connection), so the **first command after the robot connects can be rejected** with
"tool does not exist". Repeating it works. Checked with firmware commit `658910a`
(5 head tools).

## Files

| file | purpose |
|---|---|
| `bridge.py` | the bridge (stdlib + aiohttp) |
| `hermes-robot-bridge.service` | systemd unit (edit host/session) |
| `tools/fake_device.py` | fake device for latency/command tests |
| `../server-patches/` | patches applied to the local xiaozhi-server clone |
