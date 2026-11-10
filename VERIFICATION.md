# Clean-copy verification — 10 November 2026

Run from the clean folder using GCC 15.2.0, GNU make 4.4.1, Python 3.14.4,
and GNU binutils. All commands below completed with exit status 0:

| Command | Evidence |
|---|---|
| `make` | PF/RM/AM/index sources compiled and linked; no bundled binaries used |
| `make check` | Full PF/RM suites, error injection, 16 import variants and 2 oversize rejections; 4 AM regressions, 58 index cases, and all 3 original AM output sequences |
| `make sanitize` | Same focused AM/index checks under ASan/UBSan; no detected sanitizer errors; leak checking disabled by inherited target |
| `make sample` | Regenerated the byte-identical 240-record synthetic fixture |
| `python3 toydb/pflayer/experiments/run.py --output toydb/pflayer/experiments/public-results` | 40 runs; all final values validated |
| `python3 toydb/rmlayer/experiments/run.py` | 4 measured runs; exact record bytes and storage accounting validated |
| `python3 toydb/indexing/experiments/run.py` | 3 methods × 3 repetitions; 87 stage rows, source bytes/RIDs/point/range results validated |

Legacy supplied AM tests exited 12 in the ordinary build, and 0 in the sanitizer
build. Their exact expected output sequences passed independently of those
legacy exit values. This behavior is disclosed in the README.

Post-verification cleanup removed compiled ELF programs, objects, and build/run
logs from this copy. Databases used by checks were isolated under temporary
directories. Result CSVs/tables/configuration snapshots were retained. Generated
RM snapshot absolute dataset paths were omitted; relative configs and dataset
SHA-256 values preserve reproducibility. The root README's local links resolve.
Every retained C source/header and test runner matches the verified archive.
The sample has 240 records and 237 distinct keys, each exactly eight bytes.
No supplied table, ELF binary, object, generated database, environment, cache,
archive, or Git metadata is present. The original working tree/archive was not
modified. `MANIFEST.json` contains current SHA-256 hashes for every tracked
repository file except MANIFEST.json itself. Source, tests, experimental results, and original documents were not
changed by that update, and their recorded hashes still match.

Publication basis: the owner reports IIT Jodhpur institute policy permitting
students to publicly share academic assignments and projects for resumes and
professional portfolios. The official text/reference was not supplied or
independently verified. This records the reported purpose-specific permission,
not a general open-source license or confirmed coverage of third-party framework
material. Successful tests establish no publication rights. All 24 original
course tables remain excluded; the public dataset is synthetic. Historical
course-data RM/index measurements remain distinct from public synthetic runs.
See REDISTRIBUTION.md for scope and evidence limits.
