"""Bounded real-shot comparison against one owned offline dummy; restores health."""
import json
import argparse
import math
from pathlib import Path
import re
import time

from r5flowstate_agent import request

parser=argparse.ArgumentParser()
parser.add_argument("--target-array-index",type=int,default=1)
parser.add_argument("--only-phase",choices=("disabled_visible","enabled_visible","enabled_stone_occluded"))
parser.add_argument("--output",type=Path)
parser.add_argument("--aim-offset-deg",type=float,default=8)
parser.add_argument("--correction-cone",type=float,default=10)
args=parser.parse_args()
if not 0<=args.target_array_index<32:parser.error("Target array index outside bounded test range")
if not 0<=args.aim_offset_deg<=180 or not 0<=args.correction_cone<=180:parser.error("Angles outside test bounds")
TARGET=f"GetNPCArray()[{args.target_array_index}]"
PLAYER="GetPlayerArray()[0]"
WEAPON=PLAYER+".GetMainWeapons()[0]"

def evaluate(code):
    return request("server",None,script=code)

def read_vector(expression):
    value=evaluate(f"AgentLink_Reply(string({expression}))")["reply"]
    result=[float(x) for x in re.findall(r"[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?",value)]
    if len(result)!=3:raise RuntimeError("Invalid vector reply")
    return result

def phase(name,enabled,position):
    request("client",f"-attack;local_aim_enable 0;local_no_recoil 1;local_bullet_assist {int(enabled)};local_bullet_angle {args.correction_cone}")
    evaluate(PLAYER+f".SetOrigin(<{','.join(map(str,position))}>)")
    time.sleep(0.25)
    eye,point=read_vector(PLAYER+".EyePosition()"),read_vector(TARGET+".EyePosition()")
    delta=[b-a for a,b in zip(eye,point)]
    pitch=-math.degrees(math.atan2(delta[2],math.hypot(delta[0],delta[1])))
    yaw=math.degrees(math.atan2(delta[1],delta[0]))-args.aim_offset_deg
    evaluate(PLAYER+f".SnapEyeAngles(<{pitch:.6f},{yaw:.6f},0>)")
    evaluate(TARGET+".SetHealth(10000)")
    evaluate(TARGET+f".SetShieldHealth({TARGET}.GetShieldHealthMax())")
    evaluate(WEAPON+f".SetWeaponPrimaryClipCount({WEAPON}.GetWeaponSettingInt(eWeaponVar.ammo_clip_size))")
    time.sleep(0.25)
    flags=int(evaluate(f"AgentLink_Reply(format(\"%d\",{PLAYER}.GetOfflineTrainingFlags()))")["reply"])
    if flags!=(3 if enabled else 1):raise RuntimeError(f"Flags not synchronized: {flags}")
    trace=evaluate(f"AgentLink_Reply(format(\"%.8f %d\",TraceLine({PLAYER}.EyePosition(),{TARGET}.EyePosition(),{PLAYER},TRACE_MASK_SHOT,TRACE_COLLISION_GROUP_NONE).fraction,(TraceLine({PLAYER}.EyePosition(),{TARGET}.EyePosition(),{PLAYER},TRACE_MASK_SHOT,TRACE_COLLISION_GROUP_NONE).hitEnt=={TARGET})?1:0))")
    before=int(evaluate(f"AgentLink_Reply(format(\"%d\",{TARGET}.GetHealth()))")["reply"])
    shield_before=int(evaluate(f"AgentLink_Reply(format(\"%d\",{TARGET}.GetShieldHealth()))")["reply"])
    ammo_before=int(evaluate(f"AgentLink_Reply(format(\"%d\",{WEAPON}.GetWeaponPrimaryClipCount()))")["reply"])
    try:
        request("client","+attack")
        time.sleep(0.38)
    finally:
        release=request("client","-attack")
    time.sleep(0.2)
    after=int(evaluate(f"AgentLink_Reply(format(\"%d\",{TARGET}.GetHealth()))")["reply"])
    shield_after=int(evaluate(f"AgentLink_Reply(format(\"%d\",{TARGET}.GetShieldHealth()))")["reply"])
    ammo_after=int(evaluate(f"AgentLink_Reply(format(\"%d\",{WEAPON}.GetWeaponPrimaryClipCount()))")["reply"])
    return {"name":name,"enabled":enabled,"eye":eye,"targetEye":point,"aimAngles":[pitch,yaw],"flags":flags,"trace":trace,"healthBefore":before,"healthAfter":after,"shieldBefore":shield_before,"shieldAfter":shield_after,"damage":before-after+shield_before-shield_after,"ammoBefore":ammo_before,"ammoAfter":ammo_after,"release":release}

target_id=int(evaluate(f"AgentLink_Reply(format(\"%d\",{TARGET}.GetEntIndex()))")["reply"])
TARGET=f"GetEntByIndex({target_id})"
report={"scope":"Actual offline shots; direction fixed at launch","targetArrayIndex":args.target_array_index,"targetEntityIndex":target_id,"aimOffsetDegrees":args.aim_offset_deg,"correctionHalfAngle":args.correction_cone,"phases":[]}
original=evaluate(f"AgentLink_Reply(format(\"%d %d\",{TARGET}.GetHealth(),{TARGET}.GetMaxHealth()))")["reply"].split()
try:
    evaluate(TARGET+".SetMaxHealth(10000)")
    for name,enabled,position in [("disabled_visible",False,[-6220,-1150,-80]),("enabled_visible",True,[-6220,-1150,-80]),("enabled_stone_occluded",True,[-6399.94,-930,-80])]:
        if args.only_phase and name!=args.only_phase:continue
        report["phases"].append(phase(name,enabled,position))
finally:
    try:
        report["cleanup"]=request("client","-attack;local_no_recoil 0;local_bullet_assist 0")
        evaluate(TARGET+f".SetMaxHealth({int(original[1])})")
        evaluate(TARGET+f".SetHealth({int(original[0])})")
        evaluate(TARGET+f".SetShieldHealth({TARGET}.GetShieldHealthMax())")
        report["targetShieldCleanup"]="Restored to the dummy's normal full shield capacity"
    except Exception as error:
        report["cleanupError"]=str(error)
        raise
    finally:
        output=args.output or Path(__file__).resolve().parents[1]/"artifacts/r5flowstate-aim/weapon-training/bullet-hit-verification.json"
        output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
print(json.dumps(report,ensure_ascii=False,indent=2))
