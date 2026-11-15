"""Exercise real offline script direction and geometry traces without firing."""
import json
from pathlib import Path
import re
import time

from r5flowstate_agent import request

def vector(text):
    return [float(value) for value in re.findall(r"[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?",text)]

cases=[
    ("disabled",False,"<-6220,-1150,-10>","Normalize(<-1,0.14,-0.08>)",False),
    ("visible",True,"<-6220,-1150,-10>","Normalize(<-1,0.14,-0.08>)",True),
    ("outside_cone",True,"<-6220,-1150,-10>","<1,0,0>",False),
    ("stone_occluded",True,"<-6399.94,-930,-56>","Normalize(<0.14,-1,0.16>)",False),
]
report={"scope":"Real geometry/function tests with explicit origins; no shots in this script","cases":[]}
try:
    for name,enabled,origin,direction,should_change in cases:
        request("client",f"local_bullet_assist {int(enabled)};local_bullet_angle 10")
        time.sleep(0.35)
        original=request("server",None,script=f"AgentLink_Reply(string({direction}))")
        actual=request("server",None,script=f"AgentLink_Reply(string(TrainingWeapon_CorrectDirection(GetPlayerArray()[0].GetMainWeapons()[0],{origin},{direction})))")
        a,b=vector(original["reply"]),vector(actual["reply"])
        if len(a)!=3 or len(b)!=3:raise RuntimeError("Unexpected vector response")
        changed=sum((x-y)**2 for x,y in zip(a,b))>0.000001
        report["cases"].append({"name":name,"enabled":enabled,"origin":origin,"input":a,"output":b,"changed":changed,"expectedChange":should_change,"passed":changed==should_change})
finally:
    report["cleanup"]=request("client","local_bullet_assist 0")
    output=Path(__file__).resolve().parents[1]/"artifacts/r5flowstate-aim/weapon-training/bullet-direction-verification.json"
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
print(json.dumps(report,ensure_ascii=False,indent=2))
