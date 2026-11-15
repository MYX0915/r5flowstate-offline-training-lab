"""Extract only game UI automation calls around the observed historical exit."""
import json
import re
from pathlib import Path

records = []
sessions = Path(r"C:\Users\MYX\.codex\sessions\2026\11\15")
for path in sessions.glob("*.jsonl"):
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            try:
                entry = json.loads(line)
            except json.JSONDecodeError:
                continue
            stamp = entry.get("timestamp", "")
            if not "2026-11-15T07:50:40" <= stamp <= "2026-11-15T07:52:10":
                continue
            payload = entry.get("payload", {})
            if payload.get("type") not in ("function_call", "custom_tool_call"):
                continue
            arguments = payload.get("arguments", payload.get("input", ""))
            if not isinstance(arguments, str):
                continue
            actions = re.findall(r"sky\.(?:press_key|click|type_text|activate_window)\([^\n]{0,500}", arguments)
            if actions:
                records.append({"timestampUtc": stamp, "sessionFile": path.name, "callId": payload.get("call_id"), "tool": payload.get("name"), "actions": actions})
records.sort(key=lambda item: item["timestampUtc"])
output = Path(__file__).resolve().parents[1] / "artifacts/r5flowstate-aim/exit-trace/historical-actions.json"
output.parent.mkdir(exist_ok=True)
output.write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(records, ensure_ascii=False, indent=2))
