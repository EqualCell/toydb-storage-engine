#!/usr/bin/env python3
"""Explicit synthetic fixture for tool validation, NOT supplied student data."""
from pathlib import Path
import json

root = Path(__file__).resolve().parent
records = []
for i in range(240):
    # Deliberately invented text fields; no claim about the missing dataset's schema.
    records.append(f"synthetic-{i:04d};" + "x" * (8 + (i * 37) % 260) + f";group-{i % 7}")
(root / "synthetic.txt").write_bytes(b"\r\n".join(r.encode() for r in records))
config = {
    "dataset": "synthetic.txt", "buffer_capacity": 3, "policy": "LRU",
    "fixed_capacities": "derive", "skip_header": False, "repetitions": 1,
    "label": "SYNTHETIC validation only — independent variable-length fixture",
}
(root / "synthetic-config.json").write_text(json.dumps(config, indent=2) + "\n")
print("Created explicitly synthetic fixture and configuration")
