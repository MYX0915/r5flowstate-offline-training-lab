"""Run a bounded highlight experiment in the independent offline client."""
import json
from pathlib import Path
from r5flowstate_agent import request

root = Path(__file__).resolve().parents[1]
code = Path(__file__).with_name("r5flowstate_highlight_experiment.gnut").read_text(encoding="utf-8")
result = request("client", None, script=code)
(root / "artifacts/r5flowstate-aim/highlight-experiment.json").write_text(
    json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(result, ensure_ascii=True, indent=2))
