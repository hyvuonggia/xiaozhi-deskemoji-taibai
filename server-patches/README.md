# server-patches — local changes on top of xiaozhi-server

This folder holds the **local commits** applied to the running `xiaozhi-server` clone
(`/opt/xiaozhi-esp32-server`, upstream `xinnan-tech/xiaozhi-esp32-server` — they cannot be
pushed upstream because that repository is not ours). Purpose: rebuild the whole system
from scratch if the server host dies.

| patch | what it does |
|---|---|
| 0001 | HTTP API `/device/tool` + tracking of live device connections (lets Hermes call the robot's own MCP tools) |
| 0002 | Forward `reasoning_effort` from the provider config down to the model (cuts latency) |
| 0003 | `phowhisper_local` ASR provider (PhoWhisper-small running locally) |
| 0004 | `transducer` branch in `sherpa_onnx_local` (Vietnamese Zipformer 30M int8 model — ~30× faster ASR) |
| 0005 | Dockerfiles + `docker-compose.local.yml` + `piper_http.py` for the local stack (PhoWhisper / Piper / Vosk) |

## How to re-apply

```bash
cd /opt/xiaozhi-esp32-server/main/xiaozhi-server   # the upstream clone
git am /path/to/server-patches/000*.patch
```

Beyond the patches, these are needed as well (kept out of git because they are local
data / configuration):

* ASR model: `data/models/sherpa-onnx-zipformer-vi-30M-int8/`
  (encoder.int8.onnx, decoder.onnx, joiner.int8.onnx, tokens.txt);
* config `data/.config.yaml` (**contains API keys — never commit it**) with
  `ASR.SherpaVN`, `selected_module.ASR: SherpaVN`, `selected_module.LLM: HermesAgent`
  (the bridge) and `TTS` pointing at the local Piper service;
* backups of that config: `.bak-20261013-2230`, `.bak-20261013-2310`, `.bak-20261014`.
