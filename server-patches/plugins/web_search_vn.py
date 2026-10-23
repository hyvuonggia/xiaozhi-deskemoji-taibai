import json
import urllib.parse

import httpx
from plugins_func.register import register_function, ToolType, ActionResponse, Action

SEARX = "http://172.17.0.1:8888"

WEB_SEARCH_DESC = {
    "type": "function",
    "function": {
        "name": "web_search_vn",
        "description": (
            "Tim kiem web de lay THONG TIN MOI / SU THAT HIEN TAI: ai dang giu chuc vu, "
            "gia vang, tin tuc, ti so, san pham moi, luat moi... Dung khi cau hoi phu thuoc "
            "thoi diem hoac tri nho co the da cu. KHONG dung cho thoi tiet (get_weather_vn) "
            "hay gio/ngay (get_now)."
        ),
        "parameters": {
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": "Tu khoa tim kiem, ngan gon, tieng Viet duoc.",
                }
            },
            "required": ["query"],
        },
    },
}


@register_function("web_search_vn", WEB_SEARCH_DESC, ToolType.WAIT)
def web_search_vn(query: str):
    """Tim kiem web qua SearXNG noi bo (khong can API key)."""
    try:
        q = (query or "").strip()
        if not q:
            return ActionResponse(Action.REQLLM, "Can tu khoa tim kiem.", None)
        url = "%s/search?q=%s&format=json&language=vi-VN" % (SEARX, urllib.parse.quote(q))
        with httpx.Client(timeout=httpx.Timeout(15.0, connect=5.0)) as client:
            data = client.get(url, headers={"User-Agent": "Mozilla/5.0 HermesBot"}).json()
        lines = []
        for r in (data.get("results") or [])[:5]:
            title = (r.get("title") or "").strip()
            body = (r.get("content") or "").strip().replace("\n", " ")
            if not title:
                continue
            lines.append("%d. %s%s" % (len(lines) + 1, title, (" - " + body[:220]) if body else ""))
        if not lines:
            return ActionResponse(Action.REQLLM, "Khong tim thay ket qua cho: %s" % q, None)
        return ActionResponse(Action.REQLLM, "Ket qua web cho '%s':\n%s" % (q, "\n".join(lines)), None)
    except Exception as e:
        return ActionResponse(Action.REQLLM, "Tim kiem loi: %s" % e, None)
