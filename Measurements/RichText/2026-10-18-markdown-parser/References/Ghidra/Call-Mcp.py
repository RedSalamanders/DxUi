"""Call one explicitly named read/analysis MCP tool; never raw plugin HTTP."""
import argparse
import json
import pathlib
from McpSession import create_client

parser = argparse.ArgumentParser()
parser.add_argument("--tool", required=True)
parser.add_argument("--arguments", type=pathlib.Path, required=True)
parser.add_argument("--output", type=pathlib.Path, required=True)
args = parser.parse_args()
allowed = {"list_project_files", "list_open_programs", "get_metadata", "get_functions", "find_functions", "analysis_status", "search_strings", "run_script_inline"}
if args.tool not in allowed:
    raise RuntimeError("This evidence client permits only explicit read/decompilation tools")
arguments = json.loads(args.arguments.read_text(encoding="utf-8-sig"))
client = create_client()
def save(suffix, value):
    pathlib.Path(str(args.output) + suffix).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")
try:
    save(".initialize.txt", client.start())
    connected = client.call_tool("connect_instance", {"project": "notepad"}, timeout=30)
    save(".connect.txt", connected.raw)
    if connected.is_error:
        raise RuntimeError("MCP connection failed; preserved its result")
    catalog = client.list_tools()
    schema = next((item for item in catalog if item["name"] == args.tool), None)
    if schema is None:
        raise RuntimeError("Requested tool is not registered; do not bypass the registry")
    save(".schema.txt", schema)
    missing = set(schema["inputSchema"].get("required", [])) - set(arguments)
    if missing:
        raise RuntimeError("Missing registered tool arguments: " + ", ".join(sorted(missing)))
    result = client.call_tool(args.tool, arguments, timeout=90)
    save("", result.raw)
    if result.is_error:
        raise RuntimeError("MCP tool reports an error; preserved its exact result")
    print(f"MCP {args.tool}: completed; exact result preserved in {args.output.name}")
finally:
    save(".stderr.txt", client.stderr_tail)
    client.stop()
