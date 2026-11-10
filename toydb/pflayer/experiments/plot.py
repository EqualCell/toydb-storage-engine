#!/usr/bin/env python3
"""Plot measured CSV rows; no simulated or interpolated experiment values."""
import argparse
import csv
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", type=Path, default=Path(__file__).with_name("results"))
    args = parser.parse_args()
    output = args.results
    with (output / "results.csv").open() as file:
        rows = list(csv.DictReader(file))
    for row in rows:
        for key in ("capacity", "write_percent", "page_count", "operation_count",
                    "completed_reads", "completed_writes", "logical_requests",
                    "physical_reads", "physical_writes", "cache_hits"):
            row[key] = int(row[key])
        row["hit_rate"] = 100 * row["cache_hits"] / row["logical_requests"]
    capacities = sorted({r["capacity"] for r in rows})
    patterns = sorted({r["pattern"] for r in rows})
    plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False})
    metrics = [("physical_reads", "Physical data-page reads"),
               ("physical_writes", "Physical data-page writes (including close)"),
               ("logical_requests", "Logical page requests"),
               ("hit_rate", "Cache hit rate (%)")]
    for pattern in patterns:
        fig, axes = plt.subplots(2, 2, figsize=(12, 8), layout="constrained")
        for ax, (metric, label) in zip(axes.flat, metrics):
            for index, capacity in enumerate(capacities):
                for policy, style, marker in (("LRU", "-", "o"), ("MRU", "--", "x")):
                    group = sorted((r for r in rows if r["pattern"] == pattern
                                    and r["capacity"] == capacity and r["policy"] == policy),
                                   key=lambda r: r["write_percent"])
                    ax.plot([r["write_percent"] for r in group], [r[metric] for r in group],
                            linestyle=style, marker=marker, color=f"C{index % 10}",
                            markersize=7 if policy == "LRU" else 5,
                            markerfacecolor="none", label=f"{policy}, {capacity} frames")
            ax.set_xlabel("Write operations (%) — remaining operations are reads")
            ax.set_ylabel(label)
            ax.set_xticks(sorted({r["write_percent"] for r in rows}))
            ax.set_ylim(bottom=0)
            if metric == "hit_rate":
                ax.set_ylim(0, 100)
            else:
                ax.set_ylim(top=max(r[metric] for r in rows if r["pattern"] == pattern) * 1.12 or 1)
            ax.grid(alpha=0.25)
        axes[0, 0].legend(loc="best", fontsize=9)
        sample = next(r for r in rows if r["pattern"] == pattern)
        title = "Repeated sequential scans" if pattern == "sequential" else "Random access with a frequently reused subset"
        fig.suptitle(f"{title}\n{sample['page_count']} pages • {sample['operation_count']:,} operations • seed {sample['seed']}", fontsize=15)
        fig.savefig(output / f"{pattern}.png", dpi=160)
        fig.savefig(output / f"{pattern}.svg")
        plt.close(fig)
    # Counts are identical across policies, capacities and patterns; verify before summarizing.
    mixes = sorted({r["write_percent"] for r in rows})
    counts = []
    for mix in mixes:
        values = {(r["completed_reads"], r["completed_writes"]) for r in rows if r["write_percent"] == mix}
        if len(values) != 1:
            raise ValueError("operation counts differ between paired runs")
        counts.append(next(iter(values)))
    fig, ax = plt.subplots(figsize=(8, 5), layout="constrained")
    ax.bar(range(len(mixes)), [v[0] for v in counts], label="Completed reads", color="#2878a0")
    ax.bar(range(len(mixes)), [v[1] for v in counts], bottom=[v[0] for v in counts],
           label="Completed updates", color="#d88035")
    ax.set_xticks(range(len(mixes)), [str(m) for m in mixes])
    ax.set_xlabel("Write operations (%)")
    ax.set_ylabel("Completed workload operations")
    ax.set_title("Actual completed operations per run\nSame counts for both policies, capacities and patterns")
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.15), ncol=2)
    fig.savefig(output / "completed_operations.png", dpi=160)
    fig.savefig(output / "completed_operations.svg")
    plt.close(fig)
    lines = ["# Measured results", "", "All runs validated final page values against an in-memory reference.", "",
             "| Write % | Completed reads | Completed updates |", "|---:|---:|---:|"]
    lines += [f"| {mix} | {read} | {write} |" for mix, (read, write) in zip(mixes, counts)]
    lines += ["", "Physical reads and hit rates were unchanged across mixtures for each pattern/capacity/policy.", "",
              "| Pattern | Frames | Policy | Physical reads | Hit rate | Writes at " + " / ".join(map(str, mixes)) + "% |",
              "|---|---:|---|---:|---:|---|"]
    for pattern in patterns:
        for capacity in capacities:
            for policy in ("LRU", "MRU"):
                group = sorted((r for r in rows if r["pattern"] == pattern and r["capacity"] == capacity
                                and r["policy"] == policy), key=lambda r: r["write_percent"])
                if len({(r["physical_reads"], r["cache_hits"]) for r in group}) != 1:
                    raise ValueError("read/write labels unexpectedly changed cache behavior")
                row = group[0]
                writes = " / ".join(str(r["physical_writes"]) for r in group)
                lines.append(f"| {pattern} | {capacity} | {policy} | {row['physical_reads']} | {row['hit_rate']:.3f}% | {writes} |")
    lines += ["", "*Mixtures in this run: " + ", ".join(map(str, mixes)) + "%.", "",
              "Logical requests equal the configured operation count in every run. Header I/O and validation are excluded; close-time writes are included.", "",
              "Overlapping graph lines are intentional: page sequences do not change with read/write labels."]
    (output / "summary.md").write_text("\n".join(lines) + "\n")
    print(f"Wrote 3 PNG graphs, 3 SVG graphs and summary.md to {output}")


if __name__ == "__main__":
    main()
