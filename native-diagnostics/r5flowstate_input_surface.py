"""Read the owned offline process's input vtable; no writes or injection."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "artifacts/analysis-python"))
import pefile

parser = argparse.ArgumentParser()
parser.add_argument("--pid", required=True, type=int)
args = parser.parse_args()
exe = Path(r"D:\R5Flowstate\Game\r5apex.exe")
raw = exe.read_bytes()
if hashlib.sha256(raw).hexdigest().upper() != "3F44BF9AEC8A18E7FA578B8A4927D8D27BBA5AB8980119BE9935137ADCD04CB5":
    raise RuntimeError("Unexpected offline image")
pe = pefile.PE(data=raw, fast_load=True)
section = next(s for s in pe.sections if s.Name.startswith(b".text"))
code = section.get_data()
matches = list(re.finditer(rb"\x48\x8b\x0d....\x4c\x8d\x44\x24.\x48\x8b\x01\xff\x90", code, re.DOTALL))
if len(matches) != 1:
    raise RuntimeError(f"Input slot signature has {len(matches)} matches")
pos = matches[0].start()
slot_rva = section.VirtualAddress + pos + 7 + struct.unpack_from("<i", code, pos+3)[0]
kernel = c.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = (w.DWORD, w.BOOL, w.DWORD)
kernel.OpenProcess.restype = w.HANDLE
kernel.CloseHandle.argtypes = (w.HANDLE,)
kernel.ReadProcessMemory.argtypes = (w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t))
kernel.QueryFullProcessImageNameW.argtypes = (w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD))
kernel.K32EnumProcessModules.argtypes = (w.HANDLE, c.POINTER(w.HMODULE), w.DWORD, c.POINTER(w.DWORD))
handle = kernel.OpenProcess(0x410, False, args.pid)
if not handle:
    raise c.WinError(c.get_last_error())
try:
    image = c.create_unicode_buffer(32768)
    size = w.DWORD(len(image))
    if not kernel.QueryFullProcessImageNameW(handle, 0, image, c.byref(size)) or image.value.casefold() != str(exe).casefold():
        raise RuntimeError("Target is not the independent offline executable")
    modules = (w.HMODULE * 1024)()
    needed = w.DWORD()
    if not kernel.K32EnumProcessModules(handle, modules, c.sizeof(modules), c.byref(needed)):
        raise c.WinError(c.get_last_error())
    base = modules[0]

    def read(address, count):
        buffer = c.create_string_buffer(count)
        actual = c.c_size_t()
        if not kernel.ReadProcessMemory(handle, address, buffer, count, c.byref(actual)) or actual.value != count:
            raise c.WinError(c.get_last_error())
        return buffer.raw

    input_object = struct.unpack("<Q", read(base+slot_rva, 8))[0]
    table = struct.unpack("<Q", read(input_object, 8))[0]
    functions = struct.unpack("<40Q", read(table, 320))
    report = {"pid": args.pid, "imageBase": hex(base), "inputObject": hex(input_object), "vtable": hex(table), "slots": []}
    for index in range(24, 31):
        address = functions[index]
        in_image = base <= address < base+pe.OPTIONAL_HEADER.SizeOfImage
        report["slots"].append({"index": index, "address": hex(address), "preferredVa": hex(address-base+pe.OPTIONAL_HEADER.ImageBase) if in_image else None, "inImage": in_image})
    wrapper = functions[28]
    instruction = read(wrapper+11, 7)
    if instruction[:3] != b"\x48\x8d\x0d":
        raise RuntimeError("S21 command wrapper changed")
    concrete = wrapper+18+struct.unpack_from("<i",instruction,3)[0]
    concrete_table = struct.unpack("<Q",read(concrete,8))[0]
    query = struct.unpack("<Q",read(concrete_table+9*8,8))[0]
    report["concreteInput"] = {"object":hex(concrete),"table":hex(concrete_table),"querySlot9":hex(query),"queryPreferredVa":hex(query-base+pe.OPTIONAL_HEADER.ImageBase)}
    move = struct.unpack("<Q",read(concrete_table+4*8,8))[0]
    report["concreteInput"]["createPreferredVa"] = hex(move-base+pe.OPTIONAL_HEADER.ImageBase)
    block=read(concrete+0xa0,0x58)
    report["concreteInput"]["inputFields"]=[{"offset":hex(0xa0+n),"float":struct.unpack_from("<f",block,n)[0],"u32":hex(struct.unpack_from("<I",block,n)[0])} for n in range(0,len(block),4)]
    output = root / "artifacts/r5flowstate-aim/input-surface.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
finally:
    kernel.CloseHandle(handle)
