"""Exercise imports and both formats with byte-exact source edge cases."""
import csv
import io
from pathlib import Path
import subprocess
import sys

program, work = Path(sys.argv[1]), Path(sys.argv[2])
for name, content, count, payload, header in [
    ("empty", b"", 0, 0, 0),
    ("mixed", b"a;b\r\n\nlong;value\nlast;one", 3, 21, 0),
    ("header", b"name;id\r\n\r\na;2\n", 1, 3, 1),
    ("binary", b"x\x00;y\nlast", 2, 8, 0),
]:
    source = work / f"{name}.txt"
    source.write_bytes(content)
    for policy in ("LRU", "MRU"):
        for format_name in ("slotted", "fixed"):
            db = work / f"{name}-{policy}-{format_name}.pf"
            result = subprocess.run([str(program), str(source), str(db), format_name, "32", "1", policy, str(header)], text=True, capture_output=True)
            assert result.returncode == 0, result.stderr
            row = next(csv.DictReader(io.StringIO(result.stdout)))
            assert int(row["record_count"]) == count, row
            assert int(row["payload_bytes"]) == payload, row
            assert int(row["scan_writes"]) == 0
long_source = work / "too-long.txt"
long_source.write_bytes(b"a" * 10000)
for format_name in ("slotted", "fixed"):
    db = work / f"rejected-{format_name}.pf"
    result = subprocess.run([str(program), str(long_source), str(db), format_name, "32", "1", "LRU", "0"], capture_output=True)
    assert result.returncode != 0 and not db.exists(), "oversized import must fail before file creation"
print("PASS: 16 empty/text/binary imports, both policies/formats, plus 2 oversized preflight rejections")
