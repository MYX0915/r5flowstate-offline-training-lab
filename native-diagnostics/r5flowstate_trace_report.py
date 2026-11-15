"""Summarize observed offline exit and aim traces without inferring unlogged input."""
import hashlib
import json
import re
from datetime import datetime, timezone
from pathlib import Path

workspace = Path(__file__).resolve().parents[1]
logs = Path(r"D:\R5Flowstate\Game\platform\logs\client")
session = max(logs.iterdir(), key=lambda path: path.stat().st_mtime)
text = (session / "message.log").read_text(encoding="utf-8", errors="replace")
pattern = re.compile(r"\[TRAINING-AIM-TRACE\] t=([\d.]+) state=(\d+) held=(\d+) part=(\d+) target=(\d+) bone=(-?\d+) errorPx=([\d.]+) writes=(\d+) viewValid=(\d+) view=\(([-\d.]+) ([-\d.]+) ([-\d.]+)\)")
samples = []
for match in pattern.finditer(text):
    v = match.groups()
    samples.append(dict(time=float(v[0]), state=int(v[1]), held=bool(int(v[2])), part=int(v[3]), target=int(v[4]), bone=int(v[5]), errorPx=float(v[6]), writes=int(v[7]), viewValid=bool(int(v[8])), view=list(map(float, v[9:12]))))
held = [sample for sample in samples if sample["held"] and sample["state"] == 8]
segments = []
released = []
for sample in samples:
    if not sample["held"] and sample["state"] in (6,7) and sample["writes"]>0:
        if not released or sample["time"]-released[-1][-1]["time"]>0.5 or sample["writes"]!=released[-1][-1]["writes"]:
            released.append([])
        released[-1].append(sample)
    if not sample["held"] or sample["state"] != 8:
        continue
    if not segments or sample["time"] - segments[-1][-1]["time"] > 0.5 or sample["target"] != segments[-1][-1]["target"]:
        segments.append([])
    segments[-1].append(sample)
report = {
    "capturedUtc": datetime.now(timezone.utc).isoformat(),
    "logSession": str(session),
    "installedDllSha256": hashlib.sha256(Path(r"D:\R5Flowstate\Game\client.dll").read_bytes()).hexdigest(),
    "aimSampleCount": len(samples),
    "heldSteeringSamples": len(held),
    "heldSegments": [{"samples": len(group), "durationSeconds": group[-1]["time"] - group[0]["time"], "first": group[0], "last": group[-1], "minimumErrorPx": min(s["errorPx"] for s in group)} for group in segments],
    "releasedStableSegments": [{"samples":len(group),"durationSeconds":group[-1]["time"]-group[0]["time"],"writes":group[0]["writes"],"firstTime":group[0]["time"],"lastTime":group[-1]["time"]} for group in released if len(group)>=3],
    "navigationEvents": [line for line in text.splitlines() if "[TRAINING-INPUT]" in line],
    "exitScriptEvents": [line for line in (session / "script.log").read_text(encoding="utf-8", errors="replace").splitlines() if "[EXIT-DIAG]" in line or "ConfirmExitToDesktopDialog" in line],
    "lastAimSample": samples[-1] if samples else None,
    "limits": "Trace counters and sampled angles do not establish who generated historical input. Zero held samples means manual hold was not observed, not a passing hold test.",
}
output = workspace / "artifacts/r5flowstate-aim/exit-trace/runtime-verification.json"
output.parent.mkdir(exist_ok=True)
output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False, indent=2))
