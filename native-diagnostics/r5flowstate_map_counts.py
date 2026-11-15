"""Publish authoritative counts from the owned offline server to its native HUD."""

import argparse
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import time

from r5flowstate_agent import request

SCRIPT = r'''AgentLink_Reply(format("{\"players\":%d,\"npc\":%d,\"dummies\":%d,\"decoys\":%d}",GetPlayerArray().len(),GetNPCArray().len(),GetNPCArrayByClass("npc_dummie").len(),GetPlayerDecoyArray().len()))'''


def monitor(pid: int, log_path: Path) -> None:
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.QueryFullProcessImageNameW.argtypes = (wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD))
    kernel.WaitForSingleObject.argtypes = (wintypes.HANDLE, wintypes.DWORD)
    kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
    handle = kernel.OpenProcess(0x100000 | 0x1000, False, pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        path = ctypes.create_unicode_buffer(32768)
        size = wintypes.DWORD(len(path))
        if not kernel.QueryFullProcessImageNameW(handle, 0, path, ctypes.byref(size)):
            raise ctypes.WinError(ctypes.get_last_error())
        if path.value.casefold() != r"D:\R5Flowstate\Game\r5apex.exe".casefold():
            raise RuntimeError("Refusing a client outside the independent R5Flowstate installation")
        previous = None
        last_log = 0.0
        with log_path.open("a", encoding="utf-8", buffering=1) as log:
            while True:
                state = kernel.WaitForSingleObject(handle, 0)
                if state == 0:
                    break
                if state != 258:
                    raise ctypes.WinError(ctypes.get_last_error())
                try:
                    result = request("server", None, SCRIPT)
                    if result.get("error") or not isinstance(result.get("reply"), str):
                        raise RuntimeError("Local server census did not return a result")
                    counts = json.loads(result["reply"])
                    values = [counts[key] for key in ("players", "npc", "dummies", "decoys")]
                    if any(type(value) is not int or not 0 <= value <= 65536 for value in values):
                        raise ValueError("Invalid server entity count")
                    if values[2] > values[1]:
                        raise ValueError("Dummy count exceeds NPC count")
                    sent = request("client", "local_entity_mapcounts " + " ".join(map(str, values)))
                    if sent.get("error") or not sent.get("ran"):
                        raise RuntimeError("Native HUD did not accept the count update")
                    if counts != previous or time.monotonic() - last_log > 60:
                        log.write(json.dumps({"time": time.time(), "client_pid": pid, "counts": counts}) + "\n")
                        previous = counts
                        last_log = time.monotonic()
                except (OSError, RuntimeError, ValueError, KeyError) as error:
                    if time.monotonic() - last_log > 10:
                        log.write(json.dumps({"time": time.time(), "error": str(error)}) + "\n")
                        last_log = time.monotonic()
                time.sleep(1)
            log.write(json.dumps({"time": time.time(), "stopped": "client exited"}) + "\n")
    finally:
        kernel.CloseHandle(handle)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-pid", type=int, required=True)
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    monitor(args.client_pid, args.log)
