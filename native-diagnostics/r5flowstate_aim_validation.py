"""Record body-part selection and released-RMB states from the offline SDK."""
import json
import argparse
import time
from pathlib import Path
from r5flowstate_agent import request

parser=argparse.ArgumentParser()
parser.add_argument("--output",type=Path)
arguments=parser.parse_args()
results = []
for part in range(5):
    request("client", f"local_aim_part {part}")
    time.sleep(0.25)
    snapshot = request("client", "local_aim_dump")
    results.append({"part": part, "response": snapshot})
request("client", "local_aim_part 0")
path = arguments.output or Path(__file__).resolve().parents[1] / "artifacts/r5flowstate-aim/body-part-runtime.json"
path.write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(results, ensure_ascii=True, indent=2))
