"""Read-only ELF signature and Panorama ID checks; no game process is launched."""
import mmap
import re
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

patterns = {
    "CreateEntityByName": "48 8d 05 ?? ?? ?? ?? 55 48 89 fa",
    "DispatchSpawn": "48 85 ff 74 ?? 55 48 89 e5 41 55 41 54 49 89 fc",
    "RemoveEntity": "48 89 fe 48 85 ff 74 ?? 48 8d 05 ?? ?? ?? ?? 48",
}
with open(sys.argv[1], "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as data:
    assert data[:6] == b"\x7fELF\x02\x01", "expected ELF64 little-endian"
    phoff = struct.unpack_from("<Q", data, 32)[0]
    entsize, count = struct.unpack_from("<HH", data, 54)
    ranges = []
    for n in range(count):
        kind, flags, offset, vaddr, _, filesz, _, _ = struct.unpack_from("<IIQQQQQQ", data, phoff + n * entsize)
        if kind == 1 and flags & 1:
            ranges.append((offset, vaddr, filesz))
    for name, pattern in patterns.items():
        regex = re.compile(b"".join(b"." if b == "??" else re.escape(bytes.fromhex(b)) for b in pattern.split()), re.DOTALL)
        matches = [vaddr + m.start() - offset for offset, vaddr, length in ranges for m in regex.finditer(data, offset, offset + length)]
        assert len(matches) == 1, (name, matches)
        print(name, "unique executable match", hex(matches[0]))
root = Path(__file__).resolve().parents[1]
xml = ET.parse(root / "workshop/panorama/layout/custom_game/rtv_hud/vote.xml")
ids = [e.attrib["id"] for e in xml.iter() if "id" in e.attrib]
assert len(ids) == len(set(ids))
assert set(ids) == {"title", "status", "hint", *(f"choice{i}" for i in range(1, 7))}
assert not list(xml.iter("scripts"))
print("Panorama XML and server panel IDs agree")
