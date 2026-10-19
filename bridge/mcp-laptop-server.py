"""Laptop sensor MCP server (stdio) for the Hermes gateway.
Exposes read-only LAN senses so agent sessions (including the desk robot) can
measure things themselves instead of being spoon-fed answers. Started by the
gateway per mcp_servers config; speaks MCP over stdin/stdout.
"""
import asyncio
import json
import logging
import re
import sys

from mcp.server.mcpserver import MCPServer

logging.basicConfig(stream=sys.stderr, level=logging.WARNING)
log = logging.getLogger("mcp-laptop")

mcp = MCPServer("laptop-sensors")

LAPTOP_SSH_USER_HOST = "hyvuonggia@192.168.1.185"


async def _ssh_out(remote_cmd: str, timeout: float = 12.0) -> str | None:
    try:
        proc = await asyncio.create_subprocess_exec(
            "ssh", "-o", "ConnectTimeout=6", "-o", "BatchMode=yes",
            LAPTOP_SSH_USER_HOST, remote_cmd,
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL)
        try:
            out, _ = await asyncio.wait_for(proc.communicate(), timeout=timeout)
        except asyncio.TimeoutError:
            proc.kill()
            return None
        return out.decode("utf-8", "ignore") if proc.returncode == 0 else None
    except Exception as exc:  # noqa: BLE001
        log.warning("ssh failed: %s", exc)
        return None


@mcp.tool()
async def get_laptop_temp() -> str:
    """Do nhiet do CPU hien tai cua laptop CachyOS cua sep (qua mang LAN).
    Dung khi sep hoi nhiet do laptop / may tinh bao nhieu do, may co nong khong.
    Khong can tham so."""
    out = await _ssh_out("sensors 2>/dev/null | grep -E 'Package id 0|Core 0'")
    m = re.search(r"\+([\d.]+)\s*C", out) if out else None
    if not m:
        return "Khong doc duoc nhiet do laptop (may co the dang tat hoac mat mang)."
    return "Nhiet do CPU laptop hien tai: %s do C." % m.group(1)


@mcp.tool()
async def wiki_lookup(query: str) -> str:
    """Tra cuu kien thuc bach khoa tieng Viet (Wikipedia): dien tich, dan so, tieu su,
    dinh nghia, tong thong hien tai... Dung khi sep hoi 'la ai', 'la gi', 'bao nhieu',
    'o dau'. Khong co tin tuc/gia ca theo gio - nhung thu tuoi can cong cu khac."""
    import urllib.parse as _up
    import urllib.request as _url

    try:
        q0 = query.strip()
        # Entity extraction: "diện tích Việt Nam" -> "Việt Nam" (the country summary
        # holds area/population/capital); "tổng thống Mỹ hiện tại" -> "tổng thống Mỹ".
        q1 = re.sub(r"\s*(hiện tại|hiện nay|bây giờ|mới nhất|gần đây|năm nay)\s*$", "", q0)
        m = re.match(r"^(diện tích|dân số|thủ đô|tiền tệ|ngôn ngữ|quốc kỳ|múi giờ|khí hậu)\s+(của\s+)?(.+)$", q1)
        candidates = [m.group(3).strip(), q1, q0] if m else [q1, q0]
        seen = set()
        queries = [c for c in candidates if c and not (c in seen or seen.add(c))]

        def _get(url):
            req = _url.Request(url, headers={"User-Agent": "HermesBot/1.0"})
            with _url.urlopen(req, timeout=12) as r:
                return json.loads(r.read().decode("utf-8", "ignore"))

        async def _fetch(url):
            return await asyncio.to_thread(_get, url)

        async def _lookup(q):
            qq = _up.quote(q, safe="")
            found = await _fetch(
                "https://vi.wikipedia.org/w/api.php?action=opensearch&search=%s"
                "&limit=1&namespace=0&format=json" % qq)
            titles = found[1] if len(found) > 1 else []
            if not titles:
                return None
            page = await _fetch(
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


# SearXNG self-hosted search (LAN, no key, no quota): set SEARXNG_URL. When set, the
# web_search MCP tool uses it; otherwise the tool reports itself unavailable.
SEARXNG_URL = "http://192.168.1.106:8888"


@mcp.tool()
async def web_search(query: str) -> str:
    """Tim kiem web tuoi (gia vang, tin tuc, ti so, su kien moi...) qua may chu tim kiem
    rieng trong nha. Dung khi wiki_lookup khong co (thong tin thay doi theo gio/ngay).
    Dung tieng Viet de hoi."""
    import urllib.parse as _up2
    import urllib.request as _url2

    if not SEARXNG_URL:
        return "Tim kiem web chua cau hinh."
    try:
        url = ("%s/search?q=%s&language=vi-VN&time_range=month"
               % (SEARXNG_URL.rstrip("/"), _up2.quote(query, safe="")))

        def _get():
            req = _url2.Request(url, headers={"User-Agent": "HermesBot/1.0"})
            with _url2.urlopen(req, timeout=20) as r:
                return r.read().decode("utf-8", "ignore")

        import html as _html2

        def _clean(h):
            return _html2.unescape(re.sub(r"<.*?>", "", h, flags=re.S)).strip()

        data = await asyncio.to_thread(_get)
        # SearXNG HTML layout: <article class="result ..."><h3><a href="URL">TITLE</a></h3><p class="content">SNIPPET</p>
        arts = re.findall(r"<article[^>]*>(.*?)</article>", data, re.S)
        out = []
        for art in arts[:5]:
            tm = re.search(r"<h3[^>]*>.*?<a[^>]*href=\"([^\"]+)\"[^>]*>(.*?)</a>", art, re.S)
            if not tm:
                continue
            sm = re.search(r'<p class="content"[^>]*>(.*?)</p>', art, re.S)
            title = _clean(tm.group(2))
            if not title:
                continue
            line = "%d. %s" % (len(out) + 1, title)
            if sm and _clean(sm.group(1)):
                line += " - %s" % _clean(sm.group(1))[:200]
            out.append(line)
        if not out:
            return "Khong tim thay ket qua cho: %s" % query
        return "Ket qua tuoi cho '%s':\n" % query + "\n".join(out)
    except Exception as exc:  # noqa: BLE001
        log.warning("web_search failed: %s", exc)
        return "Tim kiem that bai, thu lai sau."


if __name__ == "__main__":
    asyncio.run(mcp.run_stdio_async())
