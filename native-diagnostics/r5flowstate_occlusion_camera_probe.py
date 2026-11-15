"""Select a temporary viewpoint using real scene traces; does not fake fill masks."""
import argparse
import json
from pathlib import Path
from r5flowstate_agent import request

parser = argparse.ArgumentParser()
parser.add_argument("phase", choices=("visible", "partial", "full"))
parser.add_argument("--target-index", type=int, default=-1)
arguments = parser.parse_args()
if not -1 <= arguments.target_index <= 65535:
    parser.error("target index must be -1 or a network entity index")
source = Path(__file__).with_name("r5flowstate_occlusion_camera.gnut")
code = source.read_text(encoding="utf-8").replace("@PHASE@", arguments.phase).replace("@TARGET@", str(arguments.target_index))
result = request("client", None, script=code)
report = source.parents[1] / "artifacts/r5flowstate-aim" / f"scene-camera-{arguments.phase}.json"
report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(result, ensure_ascii=True, indent=2))
