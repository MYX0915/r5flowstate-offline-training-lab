"""Install/revert narrowly scoped exit-menu trace messages in the offline UI scripts."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--restore", action="store_true")
args = parser.parse_args()
base = Path(r"D:\R5Flowstate\Game\platform\scripts\vscripts\ui")
archive = Path(__file__).resolve().parents[1] / "artifacts/r5flowstate-aim/exit-trace"
archive.mkdir(exist_ok=True)
manifest_path = archive / "manifest.json"
changes = {
    "menu_main.nut": [("void function OnMainMenu_NavigateBack()\n{", 'void function OnMainMenu_NavigateBack()\n{\n\tprintt("[EXIT-DIAG] source=MainMenu MENU_NAVIGATE_BACK")')],
    "menu_system.nut": [("file.exitButtonData.activateFunc = OpenConfirmExitToDesktopDialog", 'file.exitButtonData.activateFunc = void function(){printt("[EXIT-DIAG] source=SystemMenu exit button");OpenConfirmExitToDesktopDialog()}')],
    "dialogs/confirm_exit_to_desktop.nut": [
        ("void function OpenConfirmExitToDesktopDialog()\n{", 'void function OpenConfirmExitToDesktopDialog()\n{\n\tprintt("[EXIT-DIAG] OpenConfirmExitToDesktopDialog")'),
        ('\tClientCommand( "quit" )', '\tprintt("[EXIT-DIAG] Confirm callback issuing quit")\n\tClientCommand( "quit" )'),
    ],
}
if manifest_path.exists():
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    for item in manifest:
        path = base / item["relative"]
        expected = item["patchedSha256"]
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f"Installed script changed; refusing to overwrite {path}")
    if args.restore:
        for item in manifest:
            (base / item["relative"]).write_bytes((archive / item["backup"]).read_bytes())
        print("Exit trace scripts restored; restart the client to reload UI scripts.")
    else:
        print("Matching exit traces already installed.")
else:
    if args.restore:
        raise RuntimeError("No trace manifest to restore")
    planned = []
    for relative, replacements in changes.items():
        path = base / relative
        raw = path.read_bytes()
        newline = "\r\n" if b"\r\n" in raw else "\n"
        text = raw.decode("utf-8-sig").replace("\r\n", "\n")
        for old, new in replacements:
            if text.count(old) != 1:
                raise RuntimeError(f"Expected one patch location in {path}")
            text = text.replace(old, new, 1)
        patched = text.replace("\n", newline).encode("utf-8")
        if raw.startswith(b"\xef\xbb\xbf"):
            patched = b"\xef\xbb\xbf" + patched
        backup = relative.replace("/", "_") + ".original"
        planned.append((path, raw, patched, {"relative": relative, "backup": backup, "originalSha256": hashlib.sha256(raw).hexdigest(), "patchedSha256": hashlib.sha256(patched).hexdigest()}))
    # Validate every location before changing any installed script.
    for path, raw, patched, item in planned:
        (archive / item["backup"]).write_bytes(raw)
        path.write_bytes(patched)
    manifest_path.write_text(json.dumps([item for _, _, _, item in planned], indent=2), encoding="utf-8")
    print("Exit traces installed; restart the client to reload UI scripts.")
