"""Slim voice-tools MCP server (stdio) for xiaozhi-server.

Keyless, stdlib-only (+mcplib): wikipedia lookup, self-hosted SearXNG web search,
Open-Meteo weather. Lives in xiaozhi data/mcp_servers/ (persistent bind mount).
No SSH, no keys, nothing to expire.
"""
import asyncio
import json
import logging
import re
import sys

from mcp.server.mcpserver import MCPServer

logging.basicConfig(stream=sys.stderr, level=logging.WARNING)
log = logging.getLogger("mcp-voice")

mcp = MCPServer("voice-tools")

SEARXNG_URL = "http://172.17.0.1:8888"
WEATHER_LAT = 10.7626
WEATHER_LON = 106.6602


def _http_get_json(url, timeout=15):
    import urllib.request as _url
    req = _url.Request(url, headers={"User-Agent": "HermesBot/1.0"})
    with _url.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8", "ignore"))


def _http_get_text(url, timeout=20):
    import urllib.request as _url
    req = _url.Request(url, headers={"User-Agent": "HermesBot/1.0"})
    with _url.urlopen(req, timeout=timeout) as r:
        return r.read().decode("utf-8", "ignore")


@mcp.tool()
async def wiki_lookup(query: str) -> str:
    """Tra cuu kien thuc bach khoa tieng Viet (Wikipedia): dien tich, dan so, tieu su,
    dinh nghia... Dung khi sep hoi 'la ai', 'la gi', 'bao nhieu'. Khong co tin tuoi."""
    import urllib.parse as _up
    try:
        q0 = query.strip()
        q1 = re.sub(r"\s*(hiện tại|hiện nay|bây giờ|mới nhất|gần đây|năm nay)\s*$", "", q0)
        m = re.match(r"^(diện tích|dân số|thủ đô|tiền tệ|ngôn ngữ|quốc kỳ|múi giờ|khí hậu)\s+(của\s+)?(.+)$", q1)
        candidates = [m.group(3).strip(), q1, q0] if m else [q1, q0]
        queries = list(dict.fromkeys(c for c in candidates if c))

        async def _lookup(q):
            found = await asyncio.to_thread(
                _http_get_json,
                "https://vi.wikipedia.org/w/api.php?action=opensearch&search=%s"
                "&limit=1&namespace=0&format=json" % _up.quote(q, safe=""))
            titles = found[1] if len(found) > 1 else []
            if not titles:
                return None
            page = await asyncio.to_thread(
                _http_get_json,
                "https://vi.wikipedia.org/api/rest_v1/page/summary/%s"
                % _up.quote(titles[0].replace(" ", "_"), safe=""))
            extract = (page.get("extract") or "").strip()
            if not extract:
                return None
            if len(extract) > 600:
                cut = extract[:600]
                dot = cut.rfind(". ")
                extract = (cut[:dot + 1] if dot > 200 else cut) + " (da rut gon)"
            return "Theo Wikipedia - %s: %s" % (titles[0], extract)

        for q in queries:
            hit = await _lookup(q)
            if hit:
                return hit
        return "Khong tim thay bai viet cho: %s" % query
    except Exception as exc:  # noqa: BLE001
        log.warning("wiki_lookup failed: %s", exc)
        return "Tra cuu that bai, thu lai sau."


@mcp.tool()
async def web_search(query: str) -> str:
    """Tim kiem web tuoi (gia vang, tin tuc, ti so...) qua may tim kiem rieng trong nha.
    Dung khi wiki_lookup khong co (thong tin thay doi theo gio/ngay)."""
    import html as _html
    import urllib.parse as _up2
    try:
        url = ("%s/search?q=%s&language=vi-VN&time_range=month"
               % (SEARXNG_URL.rstrip("/"), _up2.quote(query, safe="")))

        async def _get():
            return await asyncio.to_thread(_http_get_text, url)

        data = await _get()
        arts = re.findall(r"<article[^>]*>(.*?)</article>", data, re.S)
        out = []
        for art in arts[:5]:
            tm = re.search(r"<h3[^>]*>.*?<a[^>]*href=\"([^\"]+)\"[^>]*>(.*?)</a>", art, re.S)
            if not tm:
                continue
            title = _html.unescape(re.sub(r"<.*?>", "", tm.group(2), flags=re.S)).strip()
            if not title:
                continue
            sm = re.search(r'<p class="content"[^>]*>(.*?)</p>', art, re.S)
            line = "%d. %s" % (len(out) + 1, title)
            if sm:
                snip = _html.unescape(re.sub(r"<.*?>", "", sm.group(1), flags=re.S)).strip()
                if snip:
                    line += " - %s" % snip[:200]
            out.append(line)
        if not out:
            return "Khong tim thay ket qua cho: %s" % query
        return "Ket qua tuoi cho '%s':\n" % query + "\n".join(out)
    except Exception as exc:  # noqa: BLE001
        log.warning("web_search failed: %s", exc)
        return "Tim kiem that bai, thu lai sau."


_WMO_VI = {
    0: "troi quang", 1: "troi quang", 2: "may rai rac", 3: "nhieu may",
    45: "suong mu", 48: "suong mu", 51: "mua phun", 53: "mua phun", 55: "mua phun",
    61: "mua nho", 63: "mua vua", 65: "mua to", 80: "mua rao", 81: "mua rao",
    82: "mua rat to", 95: "dong", 96: "dong kem mua da", 99: "dong kem mua da",
}


@mcp.tool()
async def get_weather() -> str:
    """Xem thoi tiet NGOAI TROI Sai Gon hien tai + hom nay (nhiet do, mua/nang).
    Dung khi sep hoi thoi tiet, mua, nang."""
    try:
        url = ("https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
               "&current=temperature_2m,weather_code&daily=temperature_2m_max,"
               "temperature_2m_min,precipitation_probability_max&timezone=Asia%%2FHo_Chi_Minh"
               % (WEATHER_LAT, WEATHER_LON))
        data = await asyncio.to_thread(_http_get_json, url)
        cur = data.get("current") or {}
        daily = data.get("daily") or {}
        t = cur.get("temperature_2m")
        if t is None:
            return "Khong coi duoc thoi tiet luc nay."
        desc = _WMO_VI.get(cur.get("weather_code"), "nhieu may")
        tmax = (daily.get("temperature_2m_max") or [None])[0]
        tmin = (daily.get("temperature_2m_min") or [None])[0]
        rain = (daily.get("precipitation_probability_max") or [None])[0]
        parts = ["Sai Gon hien %s, khoang %.0f do" % (desc, t)]
        if tmax is not None and tmin is not None:
            parts.append("ngay %.0f den %.0f do" % (tmin, tmax))
        if rain is not None and rain >= 40:
            parts.append("kha nang mua %d%%" % rain)
        return "Thoi tiet: " + ", ".join(parts) + "."
    except Exception as exc:  # noqa: BLE001
        log.warning("get_weather failed: %s", exc)
        return "Khong coi duoc thoi tiet luc nay."


if __name__ == "__main__":
    asyncio.run(mcp.run_stdio_async())
