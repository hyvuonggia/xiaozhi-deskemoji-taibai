import httpx
from plugins_func.register import register_function, ToolType, ActionResponse, Action

GET_WEATHER_VN_DESC = {
    "type": "function",
    "function": {
        "name": "get_weather_vn",
        "description": (
            "Xem thoi tiet NGOAI TROI hien tai + hom nay o Sai Gon (nhiet do, mua/nang, "
            "kha nang mua). Dung khi sep hoi thoi tiet, mua, nang, nhiet do ngoai troi. "
            "Khong lien quan nhiet do laptop (viec do cua get_laptop_temp). Khong can tham so."
        ),
        "parameters": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
}

_WMO_VI = {
    0: "troi quang", 1: "troi quang", 2: "may rai rac", 3: "nhieu may",
    45: "suong mu", 48: "suong mu", 51: "mua phun", 53: "mua phun", 55: "mua phun",
    61: "mua nho", 63: "mua vua", 65: "mua to", 80: "mua rao", 81: "mua rao",
    82: "mua rat to", 95: "dong", 96: "dong kem mua da", 99: "dong kem mua da",
}


@register_function("get_weather_vn", GET_WEATHER_VN_DESC, ToolType.WAIT)
def get_weather_vn():
    """Thoi tiet Sai Gon qua Open-Meteo (khong can key)."""
    try:
        url = ("https://api.open-meteo.com/v1/forecast?latitude=10.7626&longitude=106.6602"
               "&current=temperature_2m,weather_code&daily=temperature_2m_max,"
               "temperature_2m_min,precipitation_probability_max&timezone=Asia%2FHo_Chi_Minh")
        with httpx.Client(timeout=httpx.Timeout(15.0, connect=5.0)) as client:
            data = client.get(url, headers={"User-Agent": "HermesBot/1.0"}).json()
        cur = data.get("current") or {}
        daily = data.get("daily") or {}
        t = cur.get("temperature_2m")
        if t is None:
            return ActionResponse(Action.REQLLM, "Khong coi duoc thoi tiet luc nay.", None)
        desc = _WMO_VI.get(cur.get("weather_code"), "nhieu may")
        tmax = (daily.get("temperature_2m_max") or [None])[0]
        tmin = (daily.get("temperature_2m_min") or [None])[0]
        rain = (daily.get("precipitation_probability_max") or [None])[0]
        parts = ["Sai Gon hien %s, khoang %.0f do" % (desc, t)]
        if tmax is not None and tmin is not None:
            parts.append("ngay %.0f den %.0f do" % (tmin, tmax))
        if rain is not None and rain >= 40:
            parts.append("kha nang mua %d%%" % rain)
        return ActionResponse(Action.REQLLM, "Thoi tiet: " + ", ".join(parts) + ".", None)
    except Exception as e:
        return ActionResponse(Action.REQLLM, "Khong coi duoc thoi tiet luc nay: %s" % e, None)
