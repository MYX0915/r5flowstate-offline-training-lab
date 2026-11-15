"""Verify one explicit offline angle probe and restore the original view."""
import json
from pathlib import Path
import time

from r5flowstate_agent import request

script = 'vector a=GetLocalClientPlayer().EyeAngles();AgentLink_Reply(format("%f %f %f",a.x,a.y,a.z))'
before = request("client", None, script=script)
angles = list(map(float, before["reply"].split()))
report = {"before": before}
try:
    report["probe"] = request("client", "local_aim_view_probe")
    time.sleep(0.4)
    report["after"] = request("client", None, script=script)
    result = list(map(float, report["after"]["reply"].split()))
    report["observedYawDeltaDegrees"] = (result[1]-angles[1]+180.0)%360.0-180.0
    report["fiveDegreeProbePassed"] = abs(report["observedYawDeltaDegrees"]-5.0)<0.05
finally:
    report["restore"] = request("server", None, script="GetPlayerArray()[0].SnapEyeAngles(<"+",".join(f"{value:.6f}" for value in angles)+">)")
    path = Path(__file__).resolve().parents[1] / "artifacts/r5flowstate-aim/angle-persistence-after-fix.json"
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False, indent=2))
