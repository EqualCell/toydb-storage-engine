# toyDB: pages, records, and B+ tree indexes

A C teaching database with a shared LRU/MRU page buffer, variable-length slotted
records, a fixed-record baseline, and three B+ tree construction methods:
existing-file insertion, incremental insertion, and direct sorted bulk loading.

**Publication basis:** the project owner reports that IIT Jodhpur institute
policy permits students to publicly share academic assignments and projects for
resumes and professional portfolios. This copy is prepared for that purpose.
The policy text/reference was not supplied or independently verified; this is
not a general open-source license or a claim that third-party rights are waived.
See [REDISTRIBUTION.md](REDISTRIBUTION.md) for scope and evidence limits.
All original supplied tables remain excluded; the public dataset is synthetic.
Original author and institutional notices are preserved; see
[ATTRIBUTION.md](ATTRIBUTION.md).

## Build and test

Run from this folder on Linux with GNU make, GCC (GNU89 support), GNU binutils
(`ld`, including GNU `--wrap`), a POSIX shell, and Python 3. No Python packages
or virtual environment are required for these commands.

```sh
make
make check
make sanitize
```

Verified with GCC 15.2.0, GNU make 4.4.1, and Python 3.14.4. `make` builds source
in `toydb/`. Tests compile isolated source copies and create their databases
under `/tmp`; test output gives the log locations. `make sanitize` runs focused
AM/index checks with AddressSanitizer and UndefinedBehaviorSanitizer; leak
checking is disabled by the inherited target. Legacy compiler warnings remain.
The three supplied AM tests' output sequences are checked explicitly: their
legacy main functions can exit 12 in ordinary builds despite correct output.

## Sample data and experiments

`data/student.txt` contains 240 invented records, four named fields, and a title
row. Field 2 is an eight-byte key: 237 distinct keys and three duplicate pairs.
It contains no copied supplied records, real names, emails, or student IDs.
Regenerate the exact fixture, then reproduce measurements with:

```sh
make sample
python3 toydb/pflayer/experiments/run.py --output toydb/pflayer/experiments/public-results
python3 toydb/rmlayer/experiments/run.py
python3 toydb/indexing/experiments/run.py
```

These commands were verified from this copy. All drivers build temporary source
copies, validate values/records/queries, and write results to each layer's
`experiments/public-results/`. Rerunning overwrites those measurements. RM and
index configs explicitly select the synthetic file via a relative path; RM's
fixed capacities are derived from its record lengths. The generator creates
three duplicate pairs intentionally. Optional plotting scripts are retained;
plotting requires matplotlib and was not part of this build/test validation.
Their inherited defaults read historical `results/`; supply their supported
arguments or adjust them when plotting new measurements.

## Results and documentation

- [Verification record](VERIFICATION.md) and [full file tree](FILE_TREE.txt).
- [Original final report](toydb/REPORT.md), marked as historical.
- [Current synthetic RM results](toydb/rmlayer/experiments/public-results/table.md)
  and [index results](toydb/indexing/experiments/public-results/table.md).
- [Current synthetic PF workload results](toydb/pflayer/experiments/public-results/results.csv).
- Original aggregate CSVs, tables, configurations, and graphs remain under each
  layer's `experiments/results/`; RM also retains `synthetic-results/` from the
  original independent synthetic experiment. These are historical evidence,
  not measurements of the new sample. Supplied-data results cannot be fully
  reproduced without independently obtaining the original data with permission.
- Layer guides: [PF](toydb/pflayer/README.md), [RM](toydb/rmlayer/README.md),
  [indexing](toydb/indexing/README.md); original PF/AM documents are retained.

The original 17,813-record run used 462 slotted pages versus 495/743/990 fixed
pages. Its bulk index used 293 pages versus 467 with ordinary insertion. The
new small sample uses 4 slotted pages versus 5/8/10 fixed pages, and 5 bulk-index
pages versus 6 with ordinary insertion. These workloads differ; tiny timings
are observations, not robust performance claims. The OS cache is not cleared.

## Scope and contents

Source and tests live in `toydb/{pflayer,rmlayer,amlayer,indexing}`. Inherited
PF/AM framework code, supplied tests, and teaching documents retain their credits.
The submitted project team's implementations/modifications, tests, experiments,
and report are distinct from that inherited material; no sole authorship of the
framework is claimed. The public synthetic fixture and its generator were added
for this clean copy. Historical RM/index `results/` measurements used the original
course dataset, which is not included. See ATTRIBUTION.md for provenance limits.
Source and test bytes are unchanged from the tested submission archive. Root
build targets delegate to `toydb/`.
Changes are limited to packaging, documentation, sample generation, and
synthetic experiment configuration/output defaults. The original native C disk
layout is ABI-dependent. No SQL, WAL, transactions, crash recovery, or concurrent
update support is claimed.

Included: C source/headers, tests and runners, Makefiles, experiment and plotting
scripts, original documentation, report, aggregate results and graphs, sample
data and its generator, attribution, permission audit, and `.gitignore`.
Excluded: all 24 supplied tables, binaries/objects/libraries, generated database
files, environments/caches, raw build/runtime logs, temporary staging files,
submission archives, obsolete submission packaging/report-generation tools,
and stale verification metadata. Curated PNG/SVG result figures are intentional
report assets; executable binaries are excluded. Local paths in retained JSON
snapshots are redacted. [PROVENANCE.json](PROVENANCE.json) identifies the tested
source archive. [MANIFEST.json](MANIFEST.json) contains current SHA-256 hashes for
every tracked repository file except MANIFEST.json itself.

No Git repository has been initialized, and no commit, push, or upload was made.
