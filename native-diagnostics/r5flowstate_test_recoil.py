"""Bounded offline firing comparison; always releases attack after each phase."""
import json
from pathlib import Path
import time

from r5flowstate_agent import request

def server_state():
    value=request("server",None,script='AgentLink_Reply(format("%d %.8f",GetPlayerArray()[0].GetOfflineTrainingFlags(),GetPlayerArray()[0].GetOfflineTrainingRecoil()))')
    fields=value["reply"].split()
    return {"flags":int(fields[0]),"punch":float(fields[1])}

def phase(enabled):
    result={"enabled":enabled,"samples":[]}
    request("server",None,script='GetPlayerArray()[0].GetMainWeapons()[0].SetWeaponPrimaryClipCount(GetPlayerArray()[0].GetMainWeapons()[0].GetWeaponSettingInt(eWeaponVar.ammo_clip_size))')
    request("client",f"local_no_recoil {int(enabled)};local_bullet_assist 0;local_aim_enable 0")
    time.sleep(0.4)
    result["before"]=server_state()
    expected=1 if enabled else 0
    if result["before"]["flags"]!=expected:
        raise RuntimeError(f"Server flags did not settle: {result['before']}")
    try:
        request("client","+attack")
        end=time.monotonic()+0.65
        while time.monotonic()<end:
            result["samples"].append(server_state())
            time.sleep(0.02)
    finally:
        result["release"]=request("client","-attack")
    result["peakServerPunch"]=max((sample["punch"] for sample in result["samples"]),default=-1)
    result["client"]=request("client","local_weapon_training_dump")
    time.sleep(0.4)
    return result

report={"scope":"Owned offline training; aim and bullet correction disabled during recoil comparison","phases":[]}
output=Path(__file__).resolve().parents[1]/"artifacts/r5flowstate-aim/weapon-training/recoil-verification.json"
try:
    for enabled in (False,True,False):
        report["phases"].append(phase(enabled))
finally:
    report["finalRelease"]=request("client","-attack;local_no_recoil 0")
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
print(json.dumps(report,ensure_ascii=False,indent=2))
