"""Consolidate observed shooting reports; never substitutes source inference for tests."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

workspace=Path(__file__).resolve().parents[1]
output=workspace/"artifacts/r5flowstate-aim/weapon-training"
read=lambda name:json.loads((output/name).read_text(encoding="utf-8-sig"))
checks={}
summary={}
for weapon in ("r301","r99"):
    recoil=read(f"recoil-{weapon}.json")["phases"]
    hits=read(f"bullet-hit-{weapon}.json")["phases"]
    checks[weapon+"_recoil_toggle"]=len(recoil)==3 and recoil[0]["peakServerPunch"]>0 and recoil[1]["peakServerPunch"]==0 and recoil[2]["peakServerPunch"]>0
    checks[weapon+"_real_hit"]=hits[0]["damage"]==0 and hits[1]["damage"]>0 and hits[1]["ammoBefore"]>hits[1]["ammoAfter"]
    summary[weapon]={"recoilPeaks":[p["peakServerPunch"] for p in recoil],"visibleDamageOffOn":[hits[0]["damage"],hits[1]["damage"]]}
safe=read("bullet-occlusion-safe.json")["phases"][0]
trace=safe["trace"]["reply"].split()
checks["safe_ground_occlusion"]=safe["damage"]==0 and safe["ammoBefore"]>safe["ammoAfter"] and float(trace[0])<0.99 and trace[1]=="0"
checks["direction_cases"]=all(case["passed"] for case in read("bullet-direction-verification.json")["cases"])
files=[]
for path in [output/"menu-weapon-training.png",output/"script-manifest.json",Path(r"D:\R5Flowstate\Game\client.dll"),Path(r"D:\R5Flowstate\Game\server.dll")]:
    with path.open("rb") as stream: digest=hashlib.file_digest(stream,"sha256").hexdigest()
    files.append({"path":str(path),"sha256":digest})
report={"recordedUtc":datetime.now(timezone.utc).isoformat(),"timezone":"Asia/Shanghai","checks":checks,"passed":all(checks.values()),"results":summary,
    "safeOcclusion":{"eye":safe["eye"],"damage":safe["damage"],"trace":safe["trace"]["reply"]},"files":files,
    "scope":"Observed offline R-301 and R-99 tests. Other wrapped weapons and long-running stability have not been fully verified.",
    "diagnosticEvents":["Boolean formatting error aborted an early probe before firing; map reloaded.","A low-height test was followed by an engine task wait; client recovered; causal attribution remains unproven; ground-height occlusion retested."]}
(output/"verification.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
print(json.dumps(report,ensure_ascii=False,indent=2))
if not report["passed"]:raise RuntimeError("One or more observed shooting checks did not pass")
