from datetime import datetime, timezone
from zoneinfo import ZoneInfo

def to_beijing_iso(dt):
    """将UTC时间转换为北京时间ISO格式"""
    if not dt:
        return None
    beijing_tz = ZoneInfo("Asia/Shanghai")
    beijing_time = dt.replace(tzinfo=timezone.utc).astimezone(beijing_tz)
    return beijing_time.isoformat()

def get_beijing_now():
    """获取当前北京时间"""
    beijing_tz = ZoneInfo("Asia/Shanghai")
    return datetime.now(beijing_tz)

def get_beijing_now_iso():
    """获取当前北京时间的ISO格式字符串"""
    return get_beijing_now().isoformat()