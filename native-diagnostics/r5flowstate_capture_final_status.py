"""Capture the independent offline SDK's current diagnostic state and evidence hashes."""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

from r5flowstate_agent import request

root = Path(__file__).resolve().parents[1]
output = root / "artifacts/r5flowstate-aim"
report = {
    "capturedUtc": datetime.now(timezone.utc).isoformat(),
    "deployment": json.loads((output / "deployment-current.json").read_text(encoding="utf-8-sig")),
    "status": request("client", "local_model_shader_status; local_aim_dump"),
    "entities": request("client", None, script='string s="";foreach(entity e in GetNPCArray())s+=format("%d context=%d;",e.GetEntIndex(),e.Highlight_GetCurrentContext());AgentLink_Reply(s)'),
    "evidence": [],
    "scope": "Independent offline R5Flowstate; static scene screenshots do not establish live multiplayer or moving-target accuracy.",
}
for name in ("highlight-native-visible.png", "highlight-native-partial.png", "highlight-native-full.png", "highlight-native-disabled.png", "menu-native-final.png", "client-native-shader.dll", "client-native-shader.pdb"):
    path = output / name
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    report["evidence"].append({"path": str(path), "sha256": digest})
destination = output / "verification-native-final.json"
destination.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False, indent=2))
