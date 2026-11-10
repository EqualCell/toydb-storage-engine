#!/usr/bin/env python3
"""Run real input comparisons; never substitute synthetic input for missing data."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def inspect_dataset(path, skip_header):
    lines = []
    blank = 0
    source_lines = 0
    with path.open("rb") as source:
        for line in source:
            source_lines += 1
            if line.endswith(b"\n"):
                line = line[:-1]
            if line.endswith(b"\r"):
                line = line[:-1]
            if not line:
                blank += 1
                continue
            if skip_header:
                skip_header = False
                continue
            lines.append(line)
    lengths = [len(line) for line in lines]
    return {
        "source_lines": source_lines, "blank_lines_skipped": blank,
        "records": len(lines), "payload_bytes": sum(lengths),
        "minimum_length": min(lengths, default=0), "maximum_length": max(lengths, default=0),
        "semicolon_field_counts": sorted({line.count(b";") + 1 for line in lines}),
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    }


def table(rows, output, label, page_size):
    lines = [f"# Measured comparison: {label}", "",
             f"Utilization = live payload / (data pages × {page_size}). Metadata pages = 0.", "",
             "| Format | L | Rep | Records | Payload B | Pages | File B | Metadata B | Padding B | Unused B | PF overhead B | Utilization | Insert ms | Scan ms |",
             "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for r in rows:
        lines.append(f"| {r['format']} | {r['max_length']} | {r['repetition']} | {r['record_count']} | {r['payload_bytes']} | {r['data_pages']} | {r['file_bytes']} | {r['record_metadata_bytes']} | {r['padding_bytes']} | {r['unused_bytes']} | {r['pf_overhead_bytes']} | {float(r['utilization_percent']):.2f}% | {float(r['insert_seconds'])*1000:.3f} | {float(r['scan_seconds'])*1000:.3f} |")
    lines += ["", "| Format | L | Rep | Insert logical | Insert reads | Insert writes | Scan logical | Scan reads | Scan writes |",
              "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for r in rows:
        lines.append(f"| {r['format']} | {r['max_length']} | {r['repetition']} | {r['insert_logical']} | {r['insert_reads']} | {r['insert_writes']} | {r['scan_logical']} | {r['scan_reads']} | {r['scan_writes']} |")
    lines += ["", "Each row uses a fresh database/process and validates all bytes and RIDs after measurement.",
              "Insertion includes final close/flush. Scan includes reopen, cursor setup, iteration and close.",
              "The OS cache is not cleared. Millisecond timings are observations, not reliable speedup claims."]
    (output / "table.md").write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=Path(__file__).with_name("config.json"))
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("public-results"))
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    dataset = Path(config["dataset"])
    if not dataset.is_absolute():
        dataset = (args.config.resolve().parent / dataset).resolve()
    if not dataset.is_file():
        parser.exit(2, f"BLOCKED: dataset not found: {dataset}. No student results generated.\n")
    for key in ("buffer_capacity", "repetitions"):
        if type(config[key]) is not int or config[key] <= 0:
            raise ValueError(f"{key} must be a positive integer")
    if config["policy"] not in ("LRU", "MRU") or type(config["skip_header"]) is not bool:
        raise ValueError("policy must be LRU/MRU and skip_header must be boolean")
    inspection = inspect_dataset(dataset, config["skip_header"])
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    config["resolved_dataset"] = str(dataset)
    (output / "dataset.json").write_text(json.dumps(inspection, indent=2) + "\n")
    root = Path(__file__).resolve().parents[2]
    rows = []
    with tempfile.TemporaryDirectory(prefix="toydb-rm-experiment-") as temp:
        work = Path(temp)
        for layer in ("pflayer", "rmlayer"):
            (work / layer).mkdir()
            for path in list((root / layer).glob("*.[ch]")) + [root / layer / "Makefile"]:
                shutil.copy2(path, work / layer / path.name)
        with (output / "build.log").open("w") as log:
            subprocess.run(["make", "-B", "benchmark"], cwd=work / "rmlayer", stdout=log, stderr=subprocess.STDOUT, check=True)
        limits = subprocess.check_output([str(work / "rmlayer/benchmark"), "--limits"], text=True)
        page_size, slotted_max, fixed_max = map(int, limits.split())
        maximum = inspection["maximum_length"]
        if maximum > slotted_max:
            raise ValueError(f"maximum record {maximum} exceeds slotted capacity {slotted_max}; no truncation")
        capacities = config["fixed_capacities"]
        if capacities == "derive":
            minimum = max(1, maximum)
            gap = max(1, minimum // 2)
            capacities = [minimum, min(fixed_max, minimum + gap), min(fixed_max, minimum + 2 * gap)]
            if len(set(capacities)) < 3:
                capacities = [minimum, minimum + 1, minimum + 2]
        if not isinstance(capacities, list) or any(type(n) is not int or not max(1, maximum) <= n <= fixed_max for n in capacities) or len(set(capacities)) < 3:
            raise ValueError(f"need at least three distinct fixed capacities >= dataset maximum and <={fixed_max}")
        config["resolved_fixed_capacities"] = capacities
        config["page_size"] = page_size
        (output / "config.json").write_text(json.dumps(config, indent=2) + "\n")
        for repetition in range(1, config["repetitions"] + 1):
            for format_name, capacity in [("slotted", 0)] + [("fixed", n) for n in capacities]:
                db = work / f"{format_name}-{capacity}-{repetition}.pf"
                command = [str(work / "rmlayer/benchmark"), str(dataset), str(db), format_name,
                           str(capacity), str(config["buffer_capacity"]), config["policy"], str(int(config["skip_header"]))]
                result = subprocess.run(command, text=True, capture_output=True)
                if result.returncode:
                    raise RuntimeError(f"{command}\n{result.stderr}")
                parsed = list(csv.DictReader(io.StringIO(result.stdout)))
                if len(parsed) != 1:
                    raise RuntimeError("expected one measured row")
                row = parsed[0]
                if int(row["record_count"]) != inspection["records"] or int(row["payload_bytes"]) != inspection["payload_bytes"]:
                    raise RuntimeError("C importer and input inspection disagree")
                row.update(repetition=repetition, dataset_sha256=inspection["sha256"], label=config["label"])
                rows.append(row)
    with (output / "results.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    table(rows, output, config["label"], page_size)
    print(f"PASS: {len(rows)} measured runs, byte-for-byte validation and reconciled accounting: {output}")


if __name__ == "__main__":
    main()
