"""Install bounded, reversible shot-direction wrappers in owned offline scripts."""
import argparse
import hashlib
import json
from pathlib import Path
import re

parser=argparse.ArgumentParser()
parser.add_argument("--restore",action="store_true")
parser.add_argument("--update",action="store_true")
args=parser.parse_args()
workspace=Path(__file__).resolve().parents[1]
weapons=Path(r"D:\R5Flowstate\Game\platform\scripts\vscripts\weapons")
archive=workspace/"artifacts/r5flowstate-aim/weapon-training"
archive.mkdir(exist_ok=True)
manifest=archive/"script-manifest.json"
if manifest.exists():
    items=json.loads(manifest.read_text(encoding="utf-8"))
    for item in items:
        if hashlib.sha256((weapons/item["name"]).read_bytes()).hexdigest()!=item["installedSha256"]:
            raise RuntimeError("Installed script changed: "+item["name"])
    if args.restore and args.update:raise RuntimeError("Choose update or restore")
    if args.update:
        item=next(item for item in items if item["name"]=="mp_weapon_rspn101.nut")
        path=weapons/item["name"]
        raw=path.read_bytes()
        text=raw.decode("utf-8-sig").replace("\r\n","\n")
        marker="\nvector function TrainingWeapon_CorrectDirection("
        if text.count(marker)!=1:raise RuntimeError("Expected one installed direction helper")
        helper=Path(__file__).with_name("r5flowstate_bullet_direction.gnut").read_text(encoding="utf-8")
        changed=text.split(marker,1)[0]+"\n"+helper.lstrip("\n")
        newline="\r\n" if b"\r\n" in raw else "\n"
        patched=changed.replace("\n",newline).encode("utf-8")
        if raw.startswith(b"\xef\xbb\xbf"):patched=b"\xef\xbb\xbf"+patched
        backup=archive/"script-manifest-before-360.json"
        if not backup.exists():backup.write_bytes(manifest.read_bytes())
        path.write_bytes(patched)
        item["installedSha256"]=hashlib.sha256(patched).hexdigest()
        manifest.write_text(json.dumps(items,indent=2),encoding="utf-8")
        print("Direction helper updated; original rollback files retained. Restart both offline engines.")
    elif args.restore:
        for item in items:
            (weapons/item["name"]).write_bytes((archive/(item["name"]+".original")).read_bytes())
        print("Original weapon scripts restored; restart both offline engines.")
    else:
        print("Matching weapon training scripts already installed.")
else:
    if args.restore:raise RuntimeError("No saved manifest")
    pattern=re.compile(r"(weapon\.(?:FireWeapon_Default|FireWeaponBullet)\(\s*attackParams\.pos\s*,\s*)attackParams\.dir(?=\s*,)")
    pending=[]
    helper=Path(__file__).with_name("r5flowstate_bullet_direction.gnut").read_text(encoding="utf-8")
    for path in weapons.glob("*.nut"):
        raw=path.read_bytes()
        source=raw.decode("utf-8-sig").replace("\r\n","\n")
        modified,count=pattern.subn(r"\1TrainingWeapon_CorrectDirection(weapon,attackParams.pos,attackParams.dir)",source)
        if path.name=="mp_weapon_rspn101.nut":
            modified="global function TrainingWeapon_CorrectDirection\n"+modified+helper
        if modified==source:continue
        linebreak="\r\n" if b"\r\n" in raw else "\n"
        patched=modified.replace("\n",linebreak).encode("utf-8")
        if raw.startswith(b"\xef\xbb\xbf"):patched=b"\xef\xbb\xbf"+patched
        pending.append((path,raw,patched,{"name":path.name,"wrappedCalls":count,"originalSha256":hashlib.sha256(raw).hexdigest(),"installedSha256":hashlib.sha256(patched).hexdigest()}))
    if not any(item[3]["name"]=="mp_weapon_rspn101.nut" for item in pending):raise RuntimeError("Shared helper host missing")
    for path,raw,patched,item in pending:
        (archive/(path.name+".original")).write_bytes(raw)
        path.write_bytes(patched)
    manifest.write_text(json.dumps([item for _,_,_,item in pending],indent=2),encoding="utf-8")
    print(json.dumps({"files":len(pending),"wrappedCalls":sum(item[3]["wrappedCalls"] for item in pending)},indent=2))
