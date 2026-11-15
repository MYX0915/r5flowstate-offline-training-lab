"""Validate the saved 360-degree real-shot comparison and deployed build hashes."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[1]
output=root/"artifacts/r5flowstate-aim/weapon-training/omni360"
shots=json.loads((output/"verified-real-shots.json").read_text(encoding="utf-8"))
phases={phase["name"]:phase for phase in shots["phases"]}
off=phases["disabled_visible"]
on=phases["enabled_visible"]
blocked=phases["enabled_stone_occluded"]
trace=blocked["trace"]["reply"].split()
checks={
    "back_facing_fixture":shots["aimOffsetDegrees"]==180 and shots["correctionHalfAngle"]==180,
    "disabled_misses":off["damage"]==0 and off["ammoBefore"]>off["ammoAfter"],
    "enabled_hits":on["damage"]>0 and on["ammoBefore"]>on["ammoAfter"],
    "occluded_target_not_damaged":blocked["damage"]==0 and blocked["ammoBefore"]>blocked["ammoAfter"] and float(trace[0])<0.99 and trace[1]=="0",
    "attack_released":all(p["release"]["ran"] and not p["release"]["error"] for p in shots["phases"]),
    "cleanup_completed":"cleanupError" not in shots and shots["cleanup"]["ran"] and not shots["cleanup"]["error"],
}
files=[]
for path in [Path(r"D:\R5Flowstate\Game\client.dll"),Path(r"D:\R5Flowstate\Game\server.dll"),output/"menu-enabled.png",output/"verified-real-shots.json"]:
    with path.open("rb") as stream:digest=hashlib.file_digest(stream,"sha256").hexdigest()
    files.append({"path":str(path),"sha256":digest})
report={"recordedUtc":datetime.now(timezone.utc).isoformat(),"timezone":"Asia/Shanghai","checks":checks,"passed":all(checks.values()),
    "damage":{"disabled":off["damage"],"enabled":on["damage"],"blocked":blocked["damage"]},"files":files,
    "limits":"Observed current machine-gun fixture. No claim of all weapons, moving targets, long range or long-running stability."}
(output/"verification.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
print(json.dumps(report,ensure_ascii=False,indent=2))
if not report["passed"]:raise RuntimeError("360-degree real-shot verification failed")
