"""Read the registered Ghidra tool catalog through MCP, not raw plugin routes.

This creates/terminates only its owned stdio bridge. It never exits Ghidra,
changes server policy, runs analysis, imports files or edits either repository.
The caller supplies an outer owned-process deadline because the repository's
conformance client's readline can block despite its logical timeout.
"""
import json
import pathlib
import subprocess
import sys
import threading

repo = pathlib.Path(r"D:\utils\ghidra-mcp")
output = pathlib.Path(__file__).parent
sys.path.insert(0, str(repo / "tests" / "conformance"))
from mcp_client import PROTOCOL_VERSION, StdioTransport

class HiddenStdioTransport(StdioTransport):
    def start(self):
        # Preserve the repository client's MCP framing; suppress only the
        # console window of this explicitly owned diagnostic bridge child.
        self.proc = subprocess.Popen(
            self.command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, env=self.env, cwd=self.cwd,
            text=True, encoding="utf-8", bufsize=1,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        threading.Thread(target=self._drain_stderr, daemon=True).start()
        result = self._request("initialize", {
            "protocolVersion": PROTOCOL_VERSION,
            "capabilities": {},
            "clientInfo": {"name": "rich-text-read-only-inspection", "version": "1.0"},
        }, timeout=60)
        self._notify("notifications/initialized", {})
        return result

client = HiddenStdioTransport(
    [str(repo / ".venv" / "Scripts" / "python.exe"), "-m", "bridge_mcp_ghidra", "--transport", "stdio"],
    cwd=str(repo),
)

def save(name, value):
    (output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")

try:
    save("initialize.receipt.txt", client.start())
    save("initial-tools.receipt.txt", client.list_tools())
    result = client.call_tool("connect_instance", {"project": "notepad"}, timeout=30)
    save("connect.receipt.txt", result.raw)
    if result.is_error:
        raise RuntimeError("MCP instance connection failed; see preserved result")
    catalog = client.list_tools()
    save("registered-tools.receipt.txt", catalog)
    wanted = {"get_metadata", "list_open_programs", "list_project_files", "get_functions", "force_decompile", "open_program", "switch_program"}
    save("selected-schemas.receipt.txt", [item for item in catalog if item["name"] in wanted])
    for name in ("get_metadata", "list_open_programs", "list_project_files"):
        schema = next((item for item in catalog if item["name"] == name), None)
        if schema is None or schema["inputSchema"].get("required"):
            continue
        result = client.call_tool(name, {}, timeout=30)
        save(name + ".receipt.txt", result.raw)
    print(f"MCP registry: {len(catalog)} tools; selected schemas preserved; no program mutation or raw HTTP call.")
finally:
    save("bridge-stderr.receipt.txt", client.stderr_tail)
    client.stop()
