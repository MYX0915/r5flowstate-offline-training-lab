"""Compile the candidate's embedded sampler through the offline SDK console."""
import json
from pathlib import Path
import re
from r5flowstate_agent import request

root = Path(__file__).resolve().parents[1]
source = root / "sources/r5flowstate-sdk/s21-r5sdk-s21-unify/src/engine/client/training_aim.cpp"
code = re.search(r'R"sq\((.*?)\)sq"', source.read_text(encoding="utf-8-sig"), re.S).group(1)
result = request("client", None, script=code)
report = root / "artifacts/r5flowstate-aim/sampler-script-probe.json"
report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(result, ensure_ascii=True, indent=2))
