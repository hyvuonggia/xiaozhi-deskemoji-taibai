"""Laptop sensor MCP server (stdio) for the Hermes gateway.

Exposes read-only LAN senses so agent sessions (including the desk robot) can
measure things themselves instead of being spoon-fed answers. Started by the
gateway per mcp_servers config; speaks MCP over stdin/stdout.
"""
import asyncio
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


if __name__ == "__main__":
    asyncio.run(mcp.run_stdio_async())
