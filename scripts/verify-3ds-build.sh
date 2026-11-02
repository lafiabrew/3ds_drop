#!/usr/bin/env bash
set -euo pipefail

ELF="build/3ds/3ds_drop.elf"
SMDH="build/3ds/3ds_drop.smdh"
THREEDSX="build/3ds/3ds_drop.3dsx"
CIA="build/3ds/3ds_drop.cia"

for f in "$ELF" "$SMDH" "$THREEDSX" "$CIA"; do
  if [[ ! -s "$f" ]]; then
    echo "FAIL: Missing or empty 3DS artifact: $f" >&2
    exit 1
  fi
done

python3 - <<'PY'
import struct

with open("build/3ds/3ds_drop.elf", "rb") as f:
    elf = f.read(64)
assert elf[:4] == b"\x7fELF", "Invalid ELF magic"
e_machine = struct.unpack("<H", elf[18:20])[0]
assert e_machine == 40, f"Expected ARM e_machine=40, got {e_machine}"
import os
elf_sz = os.path.getsize("build/3ds/3ds_drop.elf")
assert elf_sz < 450000, f"Expected stripped ELF < 450KB, got {elf_sz}"

with open("build/3ds/3ds_drop.smdh", "rb") as f:
    smdh = f.read()
assert smdh[:4] == b"SMDH", "Invalid SMDH magic"
assert len(smdh) == 0x36C0, f"Expected SMDH size 0x36C0, got {hex(len(smdh))}"

with open("build/3ds/3ds_drop.3dsx", "rb") as f:
    hdr = f.read(32)
assert hdr[:4] == b"3DSX", "Invalid 3DSX magic"

with open("build/3ds/3ds_drop.cia", "rb") as f:
    cia = f.read(64)
header_size = struct.unpack("<I", cia[:4])[0]
assert header_size == 0x2020, f"Expected CIA header size 0x2020, got {hex(header_size)}"

print("PASS: Verified 3ds_drop ARM11 ELF, SMDH (0x36C0), 3DSX, and CIA (0x2020) artifacts.")
PY
