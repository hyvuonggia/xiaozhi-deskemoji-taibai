# server-patches — thay đổi cục bộ trên xiaozhi-server

Thư mục này chứa **các commit cục bộ** đã áp lên bản clone `xiaozhi-server` đang chạy
(`/opt/xiaozhi-esp32-server`, upstream `xinnan-tech/xiaozhi-esp32-server` — không push được
lên upstream vì không phải repo của mình). Mục đích: nếu máy chủ hỏng, vẫn dựng lại được
nguyên hệ thống.

| patch | nội dung |
|---|---|
| 0001 | API HTTP `/device/tool` + theo dõi thiết bị đang kết nối (để Hermes gọi được MCP tool của robot) |
| 0002 | Chuyển tiếp `reasoning_effort` từ config xuống model (giảm độ trễ) |
| 0003 | Provider ASR `phowhisper_local` (PhoWhisper-small chạy local) |
| 0004 | Nhánh `transducer` cho `sherpa_onnx_local` (model Zipformer tiếng Việt 30M int8 — ASR nhanh ~30×) |
| 0005 | Dockerfile + `docker-compose.local.yml` + `piper_http.py` cho stack local (PhoWhisper / Piper / Vosk) |

## Cách áp lại

```bash
cd /opt/xiaozhi-esp32-server/main/xiaozhi-server   # bản clone upstream
git am /duong/dan/server-patches/000*.patch
```

Ngoài patch, cần thêm (không nằm trong git vì là dữ liệu/cấu hình riêng):

* model ASR: `data/models/sherpa-onnx-zipformer-vi-30M-int8/`
  (encoder.int8.onnx, decoder.onnx, joiner.int8.onnx, tokens.txt);
* cấu hình `data/.config.yaml` (chứa API key — **không** commit) với
  `ASR.SherpaVN`, `selected_module.ASR: SherpaVN`, `selected_module.LLM: HermesAgent`
  (bridge), `TTS` trỏ Piper local;
* `data/.config.yaml` bản backup: `.bak-20261013-2230`, `.bak-20261013-2310`, `.bak-20261014`.
