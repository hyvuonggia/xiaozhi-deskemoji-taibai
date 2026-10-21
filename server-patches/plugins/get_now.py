from datetime import datetime, timedelta, timezone
from plugins_func.register import register_function, ToolType, ActionResponse, Action

GET_NOW_DESC = {
    "type": "function",
    "function": {
        "name": "get_now",
        "description": (
            "Xem GIO + NGAY (gio Viet Nam, UTC+7). Tra ve san ca 3 moc: HOM NAY, HOM QUA, "
            "NGAY MAI (thu + ngay + thang + nam) + gio hien tai. Dung MOI KHI sep hoi gio, "
            "ngay, thu, hom nay/ngay mai/hom qua. Em chi viec doc dung dong phu hop cau hoi. "
            "Khong can tham so."
        ),
        "parameters": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
}

_WD = ["hai", "ba", "tu", "nam", "sau", "bay", "nhat"]


def _fmt(d):
    return "thu %s ngay %d thang %d nam %d" % (_WD[d.weekday()], d.day, d.month, d.year)


@register_function("get_now", GET_NOW_DESC, ToolType.WAIT)
def get_now():
    """Gio + 3 moc ngay, tieng Viet."""
    try:
        now = datetime.now(timezone(timedelta(hours=7)))
        text = ("Hom nay %s. Hom qua %s. Ngay mai %s. Bay gio %d gio %d phut (gio Viet Nam)."
                % (_fmt(now), _fmt(now - timedelta(days=1)), _fmt(now + timedelta(days=1)),
                   now.hour, now.minute))
        return ActionResponse(Action.REQLLM, text, None)
    except Exception as e:
        return ActionResponse(Action.REQLLM, "Khong coi duoc gio: %s" % e, None)
