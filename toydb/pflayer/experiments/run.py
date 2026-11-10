#!/usr/bin/env python3
"""Build from sources and run each policy/configuration in a fresh process."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import random
import shutil
import subprocess
import tempfile


def validate(config):
    for key in ("page_count", "operation_count", "seed", "hot_pages", "hot_percent"):
        if type(config[key]) is not int:
            raise ValueError(f"{key} must be an integer")
    pages, count = config["page_count"], config["operation_count"]
    if not 1 <= count <= 10_000_000 or not 2 <= pages <= 1_000_000:
        raise ValueError("use 2..1000000 pages and 1..10000000 operations")
    if not config["buffer_sizes"] or any(type(n) is not int or not 1 <= n < pages
                                         for n in config["buffer_sizes"]):
        raise ValueError("buffer sizes must be positive and smaller than page_count")
    if not config["write_percentages"] or any(type(p) is not int or not 0 <= p <= 100
                                               or count * p % 100
                                               for p in config["write_percentages"]):
        raise ValueError("write percentages must give exact integer operation counts")
    if not 1 <= config["hot_pages"] < pages or not 0 <= config["hot_percent"] <= 100:
        raise ValueError("invalid hot subset configuration")
    if not config["access_patterns"] or any(p not in ("sequential", "hot_random")
                                            for p in config["access_patterns"]):
        raise ValueError("patterns: sequential, hot_random")
    for key in ("buffer_sizes", "write_percentages", "access_patterns"):
        if len(set(config[key])) != len(config[key]):
            raise ValueError(f"duplicate entries in {key}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=Path(__file__).with_name("config.json"))
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("results"))
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    validate(config)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "config.json").write_text(json.dumps(config, indent=2) + "\n")
    source = Path(__file__).resolve().parent.parent
    rows = []
    with tempfile.TemporaryDirectory(prefix="toydb-experiment-") as temp:
        work = Path(temp)
        for path in list(source.glob("*.c")) + list(source.glob("*.h")) + [source / "Makefile"]:
            shutil.copy2(path, work / path.name)
        with (output / "build.log").open("w") as log:
            subprocess.run(["make", "-B", "workload"], cwd=work, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        count, pages = config["operation_count"], config["page_count"]
        order = list(range(count))
        random.Random(config["seed"] + 1).shuffle(order)
        for pattern in config["access_patterns"]:
            rng = random.Random(config["seed"])
            sequence = []
            for i in range(count):
                if pattern == "sequential":
                    page = i % pages
                elif rng.randrange(100) < config["hot_percent"]:
                    page = rng.randrange(config["hot_pages"])
                else:
                    page = rng.randrange(config["hot_pages"], pages)
                sequence.append(page)
            for percent in config["write_percentages"]:
                updates = set(order[:count * percent // 100])
                trace = "".join(f"{page} {int(i in updates)}\n" for i, page in enumerate(sequence))
                trace_path = work / "trace.txt"
                trace_path.write_text(trace)
                trace_hash = hashlib.sha256(trace.encode()).hexdigest()
                for capacity in config["buffer_sizes"]:
                    pair = []
                    for policy in ("LRU", "MRU"):
                        with tempfile.TemporaryDirectory(dir=work, prefix="run-") as run_dir:
                            command = [str(work / "workload"), str(capacity), policy, str(pages),
                                       str(count), str(config["seed"]), pattern, str(percent), str(trace_path)]
                            result = subprocess.run(command, cwd=run_dir, text=True, capture_output=True)
                            if result.returncode:
                                raise RuntimeError(f"{command}\n{result.stderr}")
                        row = list(csv.DictReader(io.StringIO(result.stdout)))
                        if len(row) != 1:
                            raise RuntimeError("workload did not return exactly one result")
                        row = row[0]
                        row.update(hot_pages=config["hot_pages"], hot_percent=config["hot_percent"],
                                   trace_sha256=trace_hash)
                        rows.append(row)
                        pair.append(row)
                    if pair[0]["read_checksum"] != pair[1]["read_checksum"]:
                        raise RuntimeError("LRU/MRU read results differ")
    with (output / "results.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(f"PASS: {len(rows)} runs; all stored values validated; results in {output}")


if __name__ == "__main__":
    main()
