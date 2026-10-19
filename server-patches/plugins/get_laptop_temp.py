import httpx
from plugins_func.register import register_function, ToolType, ActionResponse, Action

GET_LAPTOP_TEMP_DESC = {
    "type": "function",
    "function": {
        "name": "get_laptop_temp",
        "description": (
            "Do nhiet do CPU hien tai cua laptop CachyOS cua sep (qua mang LAN). "
            "Dung khi sep hoi nhiet do laptop / may tinh bao nhieu do, may co nong khong. "
            "Khong can tham so."
        ),
        "parameters": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
}


@register_function("get_laptop_temp", GET_LAPTOP_TEMP_DESC, ToolType.WAIT)
def get_laptop_temp():
    """Read the boss's laptop CPU temperature over the LAN sensor endpoint."""
    import json as _json
    try:
        with open("/opt/xiaozhi-esp32-server/data/laptop_temp.json", "r") as f:
            cfg = _json.load(f)
        r = httpx.get(str(cfg["url"]), params={"token": cfg.get("token", "")}, timeout=15.0)
        data = r.json()
        cpu = data.get("cpu_c")
        if cpu is None:
            return ActionResponse(
                Action.REQLLM,
                "Khong doc duoc nhiet do laptop (may co the dang tat hoac mat mang).",
                None,
            )
        return ActionResponse(
            Action.REQLLM,
            "Nhiet do CPU laptop hien tai: %s do C." % cpu,
            None,
        )
    except Exception as e:
        return ActionResponse(
            Action.REQLLM,
            "Khong doc duoc nhiet do laptop: %s" % e,
            None,
        )
