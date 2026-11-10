#!/usr/bin/env python3
"""Generate invented records; no supplied table is read or transformed."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
rows = ["name;key;group;note"]
for i in range(240):
    # 237 distinct eight-byte keys, three duplicate pairs, deliberately unsorted.
    key = ((i if i < 237 else i - 237) * 73) % 237
    rows.append(f"Example-{i:04d};S{key:07d};G{i % 7};" + "x" * (8 + i % 45))
(root / "data" / "student.txt").write_text("\n".join(rows) + "\n", encoding="ascii")
print("Generated 240 synthetic records (237 distinct eight-byte keys)")
