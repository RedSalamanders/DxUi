"""Owned hidden stdio MCP transport using the repository's conformance framing."""
import pathlib
import subprocess
import sys
import threading

REPO = pathlib.Path(r"D:\utils\ghidra-mcp")
sys.path.insert(0, str(REPO / "tests" / "conformance"))
from mcp_client import PROTOCOL_VERSION, StdioTransport

class HiddenStdioTransport(StdioTransport):
    def start(self):
        self.proc = subprocess.Popen(
            self.command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, env=self.env, cwd=self.cwd,
            text=True, encoding="utf-8", bufsize=1,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        threading.Thread(target=self._drain_stderr, daemon=True).start()
        result = self._request("initialize", {
            "protocolVersion": PROTOCOL_VERSION, "capabilities": {},
            "clientInfo": {"name": "rich-text-mcp-evidence", "version": "1.0"},
        }, timeout=60)
        self._notify("notifications/initialized", {})
        return result

def create_client():
    return HiddenStdioTransport(
        [str(REPO / ".venv" / "Scripts" / "python.exe"), "-m", "bridge_mcp_ghidra", "--transport", "stdio"],
        cwd=str(REPO),
    )
