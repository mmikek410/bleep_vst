"""Fails if two installed VST3 bundles share a bundle identifier or VST3 class ID."""
import json
import pathlib
import plistlib
import re
import sys

seen = {}
for arg in sys.argv[1:]:
    bundle = pathlib.Path(arg)
    if not bundle.is_dir():
        sys.exit(f"missing bundle: {bundle}")
    plist = plistlib.loads((bundle / "Contents/Info.plist").read_bytes())
    # moduleinfo.json is JSON5-style (trailing commas); strip them for json.
    text = (bundle / "Contents/Resources/moduleinfo.json").read_text()
    module = json.loads(re.sub(r",(\s*[}\]])", r"\1", text))
    cids = {c["CID"] for c in module["Classes"]}
    names = [c["Name"] for c in module["Classes"] if c.get("Category") == "Audio Module Class"]
    print(f'{bundle.name}: CFBundleIdentifier={plist["CFBundleIdentifier"]} name={names} CIDs={sorted(cids)}')
    seen[bundle.name] = (plist["CFBundleIdentifier"], cids)

(a_id, a_cids), (b_id, b_cids) = seen.values()
if a_id == b_id:
    sys.exit("FAIL: bundle identifiers collide")
if a_cids & b_cids:
    sys.exit("FAIL: VST3 class IDs collide")
print("OK: distinct bundle identifiers and VST3 class IDs")
