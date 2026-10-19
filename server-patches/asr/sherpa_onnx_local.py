import re
import unicodedata
import time
import wave
import os
import sys
import io
from config.logger import setup_logging
from typing import Optional, Tuple, List
from core.providers.asr.dto.dto import InterfaceType
from core.providers.asr.base import ASRProviderBase

import numpy as np
import sherpa_onnx

from modelscope.hub.file_download import model_file_download

TAG = __name__
logger = setup_logging()


# 捕获标准输出
class CaptureOutput:
    def __enter__(self):
        self._output = io.StringIO()
        self._original_stdout = sys.stdout
        sys.stdout = self._output

    def __exit__(self, exc_type, exc_value, traceback):
        sys.stdout = self._original_stdout
        self.output = self._output.getvalue()
        self._output.close()

        # 将捕获到的内容通过 logger 输出
        if self.output:
            logger.bind(tag=TAG).info(self.output.strip())

LOANWORD_FIXES = None  # built below (needs full Vietnamese vowel classes)


def _vowel_class(base_variants):
    return "[" + "".join(base_variants) + "]"


_A = "a\u00e0\u00e1\u1ea0\u1ea3\u00e3\u0103\u1eb1\u1eaf\u1eb7\u1eb3\u1eb5\u00e2\u1ea7\u1ea5\u1ead\u1ea9\u1eab"
_O = "o\u00f2\u00f3\u1ecd\u1ecf\u00f5\u00f4\u1ed3\u1ed1\u1ed9\u1edb\u1ed7\u01a1\u1edd\u1edb\u1ee3\u1edf\u1ee1"
_U = "u\u00f9\u00fa\u1ee5\u1ee7\u0169\u01b0\u1eeb\u1ee9\u1ef1\u1eed\u1eef"
_I = "i\u00ec\u00ed\u1ecb\u1ec9\u0129"
_Y = "y\u1ef3\u00fd\u1ef5\u1ef7\u1ef9"

LOANWORD_FIXES = [
    ("\\bl[" + _A + "]p\\s*b[" + _A + "]p\\b", "laptop"),
    ("\\bl[" + _A + _O + "]p\\s*t[" + _O + "]p\\b", "laptop"),
    ("\\boai\\s*ph[" + _A + "]y\\b", "wifi"),
    ("\\bb[" + _U + "]u\\s*th[" + _O + "]n\\b", "bluetooth"),
    ("\\bs[" + _A + "]ch\\s*ph[" + _A + "]y\\b", "spotify"),
    ("\\bdu\\s*t[" + _U + "]p\\b", "youtube"),
    ("\\bgu\\s*g[" + _O + "]\\b", "google"),
]


def fix_loanwords(text):
    """Replace systematic English-mishearings; returns (fixed, changed)."""
    fixed = text
    for pat, word in LOANWORD_FIXES:
        fixed = re.sub(pat, word, fixed)
    return fixed, (fixed != text)



class ASRProvider(ASRProviderBase):
    def __init__(self, config: dict, delete_audio_file: bool):
        super().__init__()
        self.interface_type = InterfaceType.LOCAL
        self.model_dir = config.get("model_dir")
        self.output_dir = config.get("output_dir")
        self.model_type = config.get("model_type", "sense_voice")  # 支持 paraformer
        self.delete_audio_file = delete_audio_file

        # 确保输出目录存在
        os.makedirs(self.output_dir, exist_ok=True)

        # 初始化模型文件路径
        model_files = {
            "model.int8.onnx": os.path.join(self.model_dir, "model.int8.onnx"),
            "tokens.txt": os.path.join(self.model_dir, "tokens.txt"),
        }

        # Transducer (zipformer, vd tieng Viet): encoder/decoder/joiner rieng
        self.encoder_path = config.get("encoder") or os.path.join(self.model_dir, "encoder.int8.onnx")
        self.decoder_path = config.get("decoder") or os.path.join(self.model_dir, "decoder.onnx")
        self.joiner_path = config.get("joiner") or os.path.join(self.model_dir, "joiner.int8.onnx")

        # 下载并检查模型文件 (transducer dung file co san trong model_dir)
        if self.model_type == "transducer":
            for p in (self.encoder_path, self.decoder_path, self.joiner_path, model_files["tokens.txt"]):
                if not os.path.isfile(p):
                    raise FileNotFoundError(f"Thieu file model transducer: {p}")
            self.model_path = self.encoder_path
            self.tokens_path = model_files["tokens.txt"]
        else:
            try:
                for file_name, file_path in model_files.items():
                    if not os.path.isfile(file_path):
                        logger.bind(tag=TAG).info(f"正在下载模型文件: {file_name}")
                        model_file_download(
                            model_id="pengzhendong/sherpa-onnx-sense-voice-zh-en-ja-ko-yue",
                            file_path=file_name,
                            local_dir=self.model_dir,
                        )

                        if not os.path.isfile(file_path):
                            raise FileNotFoundError(f"模型文件下载失败: {file_path}")

                self.model_path = model_files["model.int8.onnx"]
                self.tokens_path = model_files["tokens.txt"]

            except Exception as e:
                logger.bind(tag=TAG).error(f"模型文件处理失败: {str(e)}")
                raise

        with CaptureOutput():
            if self.model_type == "paraformer":
                self.model = sherpa_onnx.OfflineRecognizer.from_paraformer(
                    paraformer=self.model_path,
                    tokens=self.tokens_path,
                    num_threads=2,
                    sample_rate=16000,
                    feature_dim=80,
                    decoding_method="greedy_search",
                    debug=False,
                )
            elif self.model_type == "transducer":
                logger.bind(tag=TAG).info(f"Loading zipformer transducer: {self.encoder_path}")
                self.model = sherpa_onnx.OfflineRecognizer.from_transducer(
                    encoder=self.encoder_path,
                    decoder=self.decoder_path,
                    joiner=self.joiner_path,
                    tokens=self.tokens_path,
                    num_threads=2,
                    sample_rate=16000,
                    feature_dim=80,
                    decoding_method="greedy_search",
                    debug=False,
                )
            else:  # sense_voice
                self.model = sherpa_onnx.OfflineRecognizer.from_sense_voice(
                    model=self.model_path,
                    tokens=self.tokens_path,
                    num_threads=2,
                    sample_rate=16000,
                    feature_dim=80,
                    decoding_method="greedy_search",
                    debug=False,
                    use_itn=True,
                )

    def read_wave(self, wave_filename: str) -> Tuple[np.ndarray, int]:
        """
        Args:
        wave_filename:
            Path to a wave file. It should be single channel and each sample should
            be 16-bit. Its sample rate does not need to be 16kHz.
        Returns:
        Return a tuple containing:
        - A 1-D array of dtype np.float32 containing the samples, which are
        normalized to the range [-1, 1].
        - sample rate of the wave file
        """

        with wave.open(wave_filename) as f:
            assert f.getnchannels() == 1, f.getnchannels()
            assert f.getsampwidth() == 2, f.getsampwidth()  # it is in bytes
            num_samples = f.getnframes()
            samples = f.readframes(num_samples)
            samples_int16 = np.frombuffer(samples, dtype=np.int16)
            samples_float32 = samples_int16.astype(np.float32)

            samples_float32 = samples_float32 / 32768
            return samples_float32, f.getframerate()

    def requires_file(self) -> bool:
        return True


# Loanword fixups: the Vietnamese transducer forces English tech words into the
# closest Vietnamese syllables ("laptop" -> "lap bap"). Map the systematic ones
# back. Only multi-syllable, distinctive readings are listed - a wrong mapping on
# a destructive command (shutdown etc.) would be dangerous, so single common words
# ("rat tot", "ap", "check") are deliberately NOT here.

    async def speech_to_text(
        self, opus_data: List[bytes], session_id: str, artifacts=None
    ) -> Tuple[Optional[str], Optional[str]]:
        """语音转文本主处理逻辑"""
        file_path = None
        try:
            if artifacts is None:
                return "", None
            file_path = artifacts.file_path

            start_time = time.time()
            s = self.model.create_stream()
            samples, sample_rate = self.read_wave(file_path)
            s.accept_waveform(sample_rate, samples)
            self.model.decode_stream(s)
            text = (s.result.text or "").strip().lower()
            text = unicodedata.normalize("NFC", text)
            text, _fixed = fix_loanwords(text)
            if _fixed:
                logger.bind(tag=TAG).info(f"ASR loanword fix: {text}")
            logger.bind(tag=TAG).info(f"ASR xong trong {time.time() - start_time:.2f}s: {text}")
            logger.bind(tag=TAG).debug(
                f"语音识别耗时: {time.time() - start_time:.3f}s | 结果: {text}"
            )

            return text, file_path

        except Exception as e:
            logger.bind(tag=TAG).error(f"语音识别失败: {e}", exc_info=True)
            return "", file_path
