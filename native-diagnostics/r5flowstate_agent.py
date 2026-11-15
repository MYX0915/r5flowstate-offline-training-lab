"""Use the independent SDK's authenticated loopback development console."""

import argparse
import base64
import json
from pathlib import Path
import socket


def request(role: str, command: str | None, script: str | None = None, log_since: int | None = None) -> dict:
    root = Path(r"D:\R5Flowstate\Game\platform")
    token_path = root / ("agentlink_client.token" if role == "client" else "agentlink.token")
    token = token_path.read_text(encoding="ascii").strip()
    if len(token) != 64 or any(c not in "0123456789abcdefABCDEF" for c in token):
        raise ValueError("Unexpected SDK token format")
    port = 37020 if role == "client" else 37019
    with socket.create_connection(("127.0.0.1", port), timeout=15) as connection:
        reader = connection.makefile("rb")
        if reader.readline(1024).strip() != f"AGENTLINK 2 {role}".encode("ascii"):
            raise RuntimeError("Unexpected SDK loopback greeting")
        connection.sendall(f"AUTH {token}\n".encode("ascii"))
        if not reader.readline(1024).startswith(b"OK"):
            raise RuntimeError("SDK loopback authentication failed")
        operation = f"CMD probe {command}" if command is not None else "INFO probe"
        if script is not None:
            encoded = base64.b64encode(script.encode("utf-8")).decode("ascii")
            operation = f"EVAL64 probe {role} {encoded}"
        if log_since is not None:
            operation = f"LOG probe {max(0, log_since)} 1000"
        if "\n" in operation or "\r" in operation:
            raise ValueError("Only one console command per request is supported")
        connection.sendall((operation + "\n").encode("utf-8"))
        line = reader.readline(8 * 1024 * 1024 + 1)
        if not line or len(line) > 8 * 1024 * 1024:
            raise RuntimeError("Missing or oversized SDK response")
        text = line.decode("utf-8").strip()
        prefix = "RESULT probe ok "
        if not text.startswith(prefix):
            raise RuntimeError("SDK request failed: " + text[:500])
        return json.loads(text[len(prefix):])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--role", choices=("client", "server"), default="client")
    operation = parser.add_mutually_exclusive_group()
    operation.add_argument("--cmd")
    operation.add_argument("--eval")
    operation.add_argument("--logs-since", type=int)
    arguments = parser.parse_args()
    print(json.dumps(request(arguments.role, arguments.cmd, arguments.eval, arguments.logs_since), ensure_ascii=False, indent=2))
