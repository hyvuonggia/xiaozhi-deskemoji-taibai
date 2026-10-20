import asyncio
import os
import re
import time
import subprocess
from pathlib import Path
from plugins_func.register import register_function, ToolType, ActionResponse, Action
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from core.connection import ConnectionHandler

PLAY_ONLINE_DESC = {
    "type": "function",
    "function": {
        "name": "play_online",
        "description": (
            "Tim va phat nhac MANG theo ten bai hat/ca si (YouTube). "
            "Dung khi sep bao 'mo bai X', 'mo nhac X', 'phat bai X' ma bai do KHONG co "
            "san trong thu vien local. Bai tai ve duoc luu cache, lan sau mo lai ngay."
        ),
        "parameters": {
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": "Ten bai hat (+ ca si neu co), vi du: Son Tung Chay Ngay Di",
                },
            },
            "required": ["query"],
        },
    },
}

_MUSIC_DIR = "/opt/xiaozhi-esp32-server/data/music"
_MAX_CACHE_MB = 500


def _enforce_cache_cap(keep=None):
    """Keep the music cache under the cap: evict oldest downloads first."""
    try:
        entries = []
        for f in os.listdir(_MUSIC_DIR):
            fp = os.path.join(_MUSIC_DIR, f)
            if os.path.isfile(fp) and fp != keep:
                entries.append((os.path.getmtime(fp), fp))
        total = sum(os.path.getsize(fp) for _, fp in entries)
        if keep and os.path.isfile(keep):
            total += os.path.getsize(keep)
        for _, fp in sorted(entries):
            if total <= _MAX_CACHE_MB * 1024 * 1024:
                break
            try:
                total -= os.path.getsize(fp)
                os.remove(fp)
            except OSError:
                pass
    except OSError:
        pass


def _safe_name(query):
    name = re.sub(r"[^\w\s-]", "", query, flags=re.UNICODE).strip()
    name = re.sub(r"\s+", " ", name)
    return (name[:60] or "bai-hat") + ".mp3"


@register_function("play_online", PLAY_ONLINE_DESC, ToolType.WAIT)
async def play_online(conn: "ConnectionHandler", query: str = None):
    """Download-on-demand music: YouTube -> mp3 cache -> device speaker."""
    if not query or not query.strip():
        return ActionResponse(Action.REQLLM, "Sep muon nghe bai gi a?", None)
    os.makedirs(_MUSIC_DIR, exist_ok=True)
    target = os.path.join(_MUSIC_DIR, _safe_name(query))
    if not os.path.exists(target):
        tmp_out = target + ".%(ext)s"
        cmd = ["yt-dlp", "--no-playlist", "--socket-timeout", "15",
               "-f", "bestaudio[ext=m4a]/bestaudio/best",
               "-o", tmp_out, "ytsearch1:%s" % query.strip()]
        try:
            proc = await asyncio.create_subprocess_exec(
                *cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try:
                await asyncio.wait_for(proc.communicate(), timeout=75)
            except asyncio.TimeoutError:
                proc.kill()
                return ActionResponse(Action.REQLLM, "Tai bai lau qua, sep thu bai khac nhe.", None)
            if proc.returncode != 0:
                return ActionResponse(Action.REQLLM, "Em khong tim thay bai do tren mang sep a.", None)
            src = target + ".m4a"
            if not os.path.exists(src):
                cands = [str(p) for p in Path(_MUSIC_DIR).glob(_safe_name(query) + ".*")]
                src = cands[0] if cands else None
                if not src:
                    return ActionResponse(Action.REQLLM, "Em khong tim thay bai do tren mang sep a.", None)
            conv = await asyncio.create_subprocess_exec(
                "ffmpeg", "-y", "-v", "error", "-i", src, "-ar", "16000", "-ac", "1",
                "-b:a", "128k", target,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try:
                await asyncio.wait_for(conv.communicate(), timeout=60)
            except asyncio.TimeoutError:
                conv.kill()
            try:
                if os.path.abspath(src) != os.path.abspath(target):
                    os.remove(src)
            except OSError:
                pass
            if not os.path.exists(target):
                return ActionResponse(Action.REQLLM, "Bai nay em tai ve loi roi sep a.", None)
        except Exception as e:
            return ActionResponse(Action.REQLLM, "Mo nhac loi: %s" % e, None)
    # Hand over to the stock local player (it streams the file to the speaker).
    from plugins_func.functions import play_music as _pm
    if not _pm.MUSIC_CACHE.get("music_dir"):
        _pm.MUSIC_CACHE["music_dir"] = os.path.abspath(_MUSIC_DIR)
        _pm.MUSIC_CACHE["music_ext"] = (".mp3", ".wav", ".p3")
        _pm.MUSIC_CACHE["music_files"] = []
        _pm.MUSIC_CACHE["scan_time"] = 0
        _pm.MUSIC_CACHE["refresh_time"] = 300
    _enforce_cache_cap(keep=target)
    await _pm.play_local_music(conn, specific_file=os.path.basename(target))
    return ActionResponse(Action.REQLLM, "Dang phat bai %s, sep thuong thuc nhe." % query.strip(), None)
