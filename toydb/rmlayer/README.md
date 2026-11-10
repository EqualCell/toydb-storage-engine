> Historical submission document. Supplied-data measurements describe the original
> private run; the public example uses synthetic data. See the root README and
> REDISTRIBUTION.md for current commands, provenance, and permission status.

# Phase 2: variable-length records and fixed-size comparison

Phase 2 is complete, including the supplied student-data comparison. All 17,813
student records passed byte-for-byte retrieval and full-scan validation in all
four storage configurations. The complete Phase 2 checks pass. See the
[real-data table](experiments/results/table.md) and
[verification report](experiments/results/verification.json). The earlier
synthetic measurements remain separately preserved.

The Phase 2 record code is in `rmlayer/`. Its implementation, tests, datasets
and measured results remain preserved. Phase 3 indexing/bulk loading is now
complete; see [the index guide](../indexing/README.md) and top-level report.

## Architecture and exact layout

RM includes `../pflayer/pf.h`: its authoritative payload size is 4096, not
AM's conflicting 1020. Every persistent page operation goes through PF. RM
adds no cache, file table, transaction system or free-space index.

A slotted data page is:

```
0                8                    boundary                 4096
+----------------+--------------------+--------------------------+
| header         | slots | free space | packed live record bytes |
+----------------+--------------------+--------------------------+
```

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 4 bytes | Format/version magic `RMP1` |
| 4 | 2 bytes | Number of directory slots, including deleted slots |
| 6 | 2 bytes | Start of packed data; initially 4096 |
| 8 + 6*s | 2 bytes | Slot s record offset |
| 10 + 6*s | 2 bytes | Slot s record length |
| 12 + 6*s | 2 bytes | State: 1 live, 0 deleted |

Deleted entries have offset=length=state=0. Fields are explicitly encoded
little-endian; structs and unaligned integer pointers are never stored in pages.
Header/slot growth is checked against the data boundary before any insertion.
A new empty page supports at most `4096 - 8 - 6 = 4082` bytes in one record.
Zero-length records are rejected. Records never span pages; files do.

No RM metadata page is needed: PF already supplies page traversal and EOF.
`RMP1` identifies data pages. Empty PF files have no format marker until their
first insertion. The layer must own its file: do not mix raw PF allocations,
other formats or AM writes into it.

## API and errors

Include `rm.h`; call `PF_Init()` once and configure capacity before allocation.

```c
int RM_CreateFile(char *name);
int RM_OpenFile(char *name, int policy);  /* returns descriptor or negative error */
int RM_CloseFile(int fd);
int RM_InsertRecord(int fd, const void *data, size_t length, RM_RID *rid);
int RM_GetRecord(int fd, RM_RID rid, void *out, size_t capacity, size_t *length);
int RM_DeleteRecord(int fd, RM_RID rid);
int RM_OpenScan(int fd, RM_Scan *scan);
int RM_GetNextRecord(RM_Scan *scan, RM_RID *rid,
                     void *out, size_t capacity, size_t *length);
int RM_CloseScan(RM_Scan *scan);
int RM_GetSpace(int fd, RM_Space *space);
void RM_PrintError(const char *message);
```

`RM_RID` has integer `page` and `slot` fields. Both are zero-based. `RM_Scan`
is a caller-owned cursor, initialized by OpenScan; callers should not edit its
fields. No global scan table or page pin is retained between calls.

Example (check every return in application code):

```c
PF_Init();
PF_SetBufferSize(1);
RM_CreateFile("students.pf");
fd = RM_OpenFile("students.pf", PF_LRU);
RM_InsertRecord(fd, "example;unchanged;text",
                sizeof("example;unchanged;text") - 1, &rid);
RM_GetRecord(fd, rid, output, sizeof(output), &length);
RM_DeleteRecord(fd, rid);
RM_CloseFile(fd);
```

Stored length is exact: there is no added NUL.
Output belongs to the caller and is copied before the page is unfixed.
`RME_TOOSMALL` reports the required length without touching the output buffer.
A scan's too-small result does not consume the record, so it can be retried.
Passing NULL with capacity 0 is a supported size query (returns TOOSMALL for a
live record); NULL with nonzero capacity is invalid.

All successful non-open operations return 0. Failures set `RMerrno` and return:

| Code | Value | Meaning |
|---|---:|---|
| RME_PF | -101 | PF failure; `PFerrno` retains its underlying cause |
| RME_INVALID | -102 | Invalid argument or zero length |
| RME_TOOLARGE | -103 | Record exceeds the format capacity |
| RME_NOTFOUND | -104 | Invalid page/slot or deleted RID |
| RME_TOOSMALL | -105 | Output capacity insufficient; length returned |
| RME_EOF | -106 | End of scan, distinct from failure |
| RME_CORRUPT | -107 | Wrong format or invalid layout fields |
| RME_SCAN | -108 | Invalid/closed scan |

Success does not clear an older RMerrno, matching PF's convention. Invalid
PF descriptors map to RME_PF/PFE_FD. No operation returns a pointer to a PF frame.

## Operations, pins and stable IDs

Insertion starts at the first existing page using PF_GetNextPage. It validates
the layout, finds the first deleted slot if any, and checks payload plus any
new slot-directory bytes. It writes only after confirming the fit. A failed
fit releases the page and continues. Only PF EOF causes allocation of a new,
explicitly zeroed and initialized page. Oversized input is rejected beforehand.

Get/delete validate the RID, retrieve one page with PF_GetThisPage and validate
the slot/live flag. Invalid/deleted access does not change data. Get copies
bytes and releases the pin. Delete builds a complete replacement layout in a
4096-byte temporary buffer: it clears the target slot, copies survivors into
packed positions, changes only their offsets, then copies the finished page
back. There is no overlapping memcpy.

Mutation calls PF_MarkDirty then PF_UnfixPage with TRUE; read/error paths unfix
FALSE. Common release code always attempts to unfix even if marking fails.
Failures are propagated. No ordinary operation needs more than one fixed page.
When PF reports an already-fixed page, RM does not release somebody else's pin.

Scans visit pages and slots in ascending order, skip deleted slots and empty
pages, copy one record and immediately unfix. Read errors can be retried without
rewinding the exhausted page. OpenScan validates the descriptor but retains
no pin. CloseScan only invalidates the cursor. Modifying or closing the file
while a scan is active is unsupported.

Example with three records of 3, 5 and 2 bytes on page 0:

| Step | Slot 0 | Slot 1 | Slot 2 | Boundary |
|---|---|---|---|---:|
| Insert A, B, C | A: offset 4093, len 3 | B: 4088, len 5 | C: 4086, len 2 | 4086 |
| Delete B, compact | A: 4093, len 3 | deleted | C: 4091, len 2 | 4091 |
| Insert D, len 4 | A unchanged | D: 4087, len 4 | C unchanged | 4087 |

A's RID remains (0,0); C's remains (0,2). Neither moving bytes nor reopening
changes those slot numbers. B's deleted RID may later identify D after reuse;
there are no generation counters. A directory is never renumbered/shrunk, and
empty data pages are not disposed. Their slots and payload space are reusable.
A previously populated empty page can retain more directory metadata than a
brand-new page and therefore have a smaller maximum contiguous record capacity.

## Fixed-size baseline

`fixed.h`/`fixed.c` provide FM_CreateFile, FM_OpenFile, FM_CloseFile,
FM_InsertRecord, FM_GetRecord, FM_OpenScan, FM_GetNextRecord, FM_CloseScan and
FM_GetSpace. `FM_File` holds the PF descriptor and configured maximum L;
`FM_Scan` holds a cursor and L. RM's errors and RID type are reused.

Layout: 8-byte header (`FMP1`, u16 L, u16 used slot count), then fixed slots.
Each occupied slot has u16 actual length, u16 live state and exactly L payload
bytes. Unused payload within that slot is zero-filled. L must be 1..4084.
There are `floor((4096-8)/(L+4))` available slots per page. Slots/pages are
validated before use, and reopening with the wrong L fails for a nonempty file.

The baseline also uses first-fit and one PF pin. It supports insertion and
reading, not deletion: the measured comparison has no deletion workload.
Its state field is always live for occupied slots. There is no claim of a
fixed-format deletion benchmark.

## Importing source text

`student.c` uses POSIX getline, which grows dynamically. It strips a terminal
LF and optional terminal CR (also a final CR), handles a final unterminated
line, and stores the remaining exact bytes. It preserves semicolons, spaces,
field order and embedded NULs. Empty lines after line-ending removal are skipped;
whitespace-only lines remain records. No schema, field position or roll-number
position is assumed.

By default all nonblank lines, including any header, are records. Set
`skip_header: true` to skip exactly the first nonblank line, only after inspecting
the actual source. Lengths are size_t until checked before encoding. Import
preflight checks every length before creating the database, so oversized lines
fail instead of being truncated. The input reference remains in memory for
byte-for-byte validation. No terminating NUL is appended to stored records.

The supplied file is `../data/student.txt`, relative to the code repository
root. The extracted `data/` directory and `toydb/` code directory are siblings.
Its first line is the explicit title `Database dummy - table student`, not a
student record or a list of column names. The real-data configuration sets
`skip_header: true` for this inspected title. It does not silently discard a
student. One trailing blank line is skipped by the documented blank-line rule.

The source has 17,815 CRLF-terminated lines: one title, 17,813 student rows and
one blank line. Every student row has 16 semicolon-separated fields, including
empty fields. Stored student rows total 1,759,011 bytes; minimum length is 91
and maximum is 108 bytes. All rows fit the 4,082-byte slotted limit without
truncation. Line endings and the title are excluded from stored payload.
The original source is 1,794,671 bytes and remains unchanged.

The supplied text exports contain table titles but no authoritative column
schema. PF/AM documentation describes APIs, not student field names. The
roll-number field is therefore unconfirmed for Phase 3. Field 2 is uniformly
eight characters but has only 17,810 distinct values across 17,813 rows;
its appearance alone is insufficient to establish the schema or a unique key.
No roll-number parsing or indexing is performed in Phase 2.

`experiments/results/dataset.json` records the runner's inspection;
`input-inspection.json` additionally records the title, CRLF handling, field
counts and fit checks. The source SHA-256 is
`89cab96fa7ee067eef0f1ee80f14a46a194d53c16086ad076cdbe2decf669d58`.

## Build, tests and direct import

Dependencies: GNU make, a GNU89-capable C compiler, Python 3, POSIX getline and
CLOCK_MONOTONIC. Failure tests use GNU linker --wrap. No plotting dependency.

From the repository root:

```sh
make -C pflayer check
make -C rmlayer check
```

Both checks rebuild copied sources in printed `/tmp/toydb-...` directories,
run with fresh test data, and preserve supplied samples and Phase 1 results.
The RM check also builds `benchmark`. To compile an isolated importer manually:

```sh
build=$(mktemp -d /tmp/toydb-rm-build.XXXXXX)
mkdir "$build/pflayer" "$build/rmlayer"
cp pflayer/*.[ch] pflayer/Makefile "$build/pflayer/"
cp rmlayer/*.[ch] rmlayer/Makefile "$build/rmlayer/"
make -B -C "$build/rmlayer" benchmark

# dataset, new output database, format, L (0 for slotted), PF capacity, policy, skip-header
"$build/rmlayer/benchmark" ../data/student.txt /tmp/new-students.pf slotted 0 3 LRU 1
"$build/rmlayer/benchmark" ../data/student.txt /tmp/new-fixed.pf fixed 108 3 LRU 1
```

The direct importer retains the database and emits one validated CSV row. Output
files must not already exist. These commands run from the code repository root;
the final argument explicitly skips the inspected dataset title.
`benchmark --limits` reports limits derived from the compiled PF/RM headers.

## Experiments and reproducibility

The checked-in `experiments/config.json` uses `../../../data/student.txt`,
resolved relative to `rmlayer/experiments/`, so it locates the sibling extracted
data directory without a machine-specific absolute path. Run from the code
repository root:

```sh
python3 rmlayer/experiments/run.py
# Custom config and separate output:
python3 rmlayer/experiments/run.py --config my-config.json --output /tmp/rm-results
```

| Config field | Meaning |
|---|---|
| dataset | Source path, relative to the config file unless absolute |
| buffer_capacity | Positive PF frame count, same for all compared formats |
| policy | LRU or MRU, same for all formats |
| fixed_capacities | List of at least three distinct valid lengths, or `"derive"` |
| skip_header | Whether to skip the first nonblank line |
| repetitions | Positive number of fresh-process repetitions |
| label | Human-readable identity of the dataset/results |

With maximum observed input length M, derive uses M, M+floor(M/2), M+2*floor(M/2),
with a minimum step of 1 and upper-bound checks. Near the limit it falls back
to M,M+1,M+2 only if all fit. Empty data uses minimum 1. Capacities are validated
against compiled limits. If three valid capacities or slotted storage cannot
fit the data, the experiment fails with an explanation, not successful rows.

Every row gets a fresh process/database and identical input order. All fixture
storage goes through PF; direct stdio is only for input text and result output.
The runner rebuilds copied C sources; it never uses bundled objects.

Insertion timing starts immediately before the insert loop and ends after
close/dirty flush. Create/open of the initially empty file and source parsing
are outside it. Counters reset just before that interval. Sequential-scan timing
starts before reopen and includes OpenScan, every GetNextRecord, CloseScan and
file close. Counters reset before reopen, so format/header validation performed
by open is included. No pages remain cached from the previous stage. This does
not make the OS cache cold. Timing uses CLOCK_MONOTONIC.

Counter snapshots are saved before validation. After measurement, the program
gets every saved RID and compares exact bytes, scans the complete file against
a reference sorted by RID, and checks every record appears exactly once. Scan
order is physical page/slot order; first-fit placement need not preserve input
order when a later short record fits an earlier page. RM_GetSpace and stat run
outside measurement, and byte-accounting identities are asserted.

Outputs: configuration snapshot, dataset inspection, build log, results.csv and
a readable table.md. Per-stage CSV includes logical requests, hits, misses and
physical reads/writes. See Phase 1 for definitions: these are file-layer data-page
operations, excluding PF headers, not SSD accesses. All insertion close writes
are included; validation does not contaminate counters.

Space categories reconcile as:

```
useful payload + record metadata + fixed padding + unused = pages * 4096
file bytes = pages * 4096 + PF overhead
PF overhead = sizeof(PFhdr_str) + pages * (sizeof(PFfpage) - 4096)
```

Metadata includes each data-page header and allocated directory/slot entries;
deleted RM entries remain metadata. Fixed padding means L-actual_length for each
occupied slot. Never-created fixed slots and the trailing page remainder count
as unused, not padding. There are zero metadata pages. On this build PF adds an
8-byte file header and 4 bytes per data page. Actual file size is checked with
stat. Main utilization is `100 * live payload / (pages * 4096)` (0 for an empty
file); file bytes are separately reported so PF overhead is not hidden.

## Actual real-student measurements

The checked-in configuration uses 3 PF frames, LRU, one repetition and fixed
maximum lengths 108, 162 and 216 bytes: the observed maximum M, 1.5M and 2M.
Each is distinct, accommodates every student row and fits the fixed format's
4,084-byte limit. Including each slot's 4 metadata bytes, these lengths allow
36, 24 and 18 slots per data page respectively.

All four runs rebuilt from source, used fresh databases/processes and inserted
the same student rows in source order. Each closed to flush dirty pages before
collecting insertion results, then reopened for the measured scan. Outside
measurement, every saved RID was retrieved and compared byte-for-byte, and a
complete scan was compared by RID and bytes to the reference, including exact
membership/count. All 17,813 records were validated in every format.

| Format | L | Pages | File bytes | Metadata bytes | Padding bytes | Unused bytes | PF overhead | Utilization | Insert ms | Scan ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| slotted | — | 462 | 1,894,208 | 110,574 | 0 | 22,767 | 1,856 | 92.95% | 2714.183 | 3.783 |
| fixed | 108 | 495 | 2,029,508 | 75,212 | 164,793 | 28,504 | 1,988 | 86.76% | 2458.410 | 3.267 |
| fixed | 162 | 743 | 3,046,308 | 77,196 | 1,126,695 | 80,426 | 2,980 | 57.80% | 3524.109 | 2.573 |
| fixed | 216 | 990 | 4,059,008 | 79,172 | 2,088,597 | 128,260 | 3,968 | 43.38% | 4805.040 | 2.661 |

There are zero metadata pages. The data-page byte totals reconcile in every
row: payload + record/page metadata + fixed padding + unused = pages * 4096.
PF overhead is separate: 8 file-header bytes plus 4 bytes per data page.
For example, slotted storage accounts for
1,759,011 + 110,574 + 0 + 22,767 = 1,892,352 data-page bytes;
adding 1,856 PF bytes gives its actual 1,894,208-byte file.
[verification.json](experiments/results/verification.json) records all four
independently checked totals.

Using actual file sizes, slotted storage saves 135,300 bytes (6.67%) against
fixed108, 1,152,100 bytes (37.82%) against fixed162 and 2,164,800 bytes (53.33%)
against fixed216. It requires 33, 281 and 528 fewer data pages respectively.
The fixed108 baseline is relatively compact because student row lengths are
close to its maximum; larger reservations add substantial padding.

First-fit still searches from the first page for every insertion. With only
three frames and hundreds of data pages, millions of logical lookups become
physical toyDB reads: fewer pages helps, but does not remove this search cost.
The slotted implementation also validates slot directories; in this run its
insertion took 2.714 s versus fixed108's 2.458 s despite using fewer pages.
Fixed162/fixed216 took 3.524/4.805 s. Sequential scans took approximately
2.6–3.8 ms. These are single-run observations with an uncleared OS cache, not
stable speedup estimates; page reads are file-layer calls, not SSD accesses.
The existing timing/flush/validation boundaries described above apply.

The complete [real results table](experiments/results/table.md) includes
per-stage logical requests and physical reads/writes; the
[CSV](experiments/results/results.csv) also includes hits/misses and source hash.
The input inspection, configuration snapshot, build diagnostics and current
Phase 2 check output are retained in `experiments/results/`.
No storage bug was exposed and no PF/RM C implementation change was necessary.

## Preserved synthetic measurements

Reproduce the separate synthetic pipeline:

```sh
python3 rmlayer/experiments/make_synthetic.py
python3 rmlayer/experiments/run.py --config rmlayer/experiments/synthetic-config.json --output rmlayer/experiments/synthetic-results
```

The generator explicitly invents 240 labelled lines, 38,740 useful bytes,
lengths 31..290, three semicolon-separated text fields, CRLF separators and no
final newline. This is not a reconstructed student dataset. Capacity=3, LRU,
one repetition. Derived fixed lengths are 290,435,580.

| Format | L | Data pages | Actual file bytes | Metadata bytes | Padding bytes | Unused bytes | Utilization |
|---|---:|---:|---:|---:|---:|---:|---:|
| Slotted | — | 10 | 41,008 | 1,520 | 0 | 700 | 94.58% |
| Fixed | 290 | 19 | 77,908 | 1,112 | 30,860 | 7,112 | 49.78% |
| Fixed | 435 | 27 | 110,708 | 1,176 | 65,660 | 5,016 | 35.03% |
| Fixed | 580 | 35 | 143,508 | 1,240 | 100,460 | 2,920 | 27.02% |

All four measured runs passed exact-byte and RID validation. The complete
[measured table](experiments/synthetic-results/table.md) contains actual timings
and per-stage logical/physical counts; [CSV](experiments/synthetic-results/results.csv)
contains all counters. These synthetic artifacts are unchanged; current
real-student results are stored separately in `experiments/results/`.

Slotted storage avoids per-record maximum-length padding but pays a 6-byte
slot directory and compaction work. Fixed storage has 4-byte slot metadata and
simple offsets, but larger L wastes payload capacity. Both implementations
perform first-fit scans: insertion can revisit many pages and incur substantial
I/O when the file exceeds the pool. In these runs insertion reads were
1179/2240/3240/4158 for slotted/fixed290/fixed435/fixed580; sequential reads were
10/19/27/35. Slotted layout validation also scans directory entries, so fewer
pages does not guarantee faster CPU time. Sub-millisecond/millisecond timings
are noisy observations, not a claimed speedup or real-student performance.

## Tests and limitations

Staged tests ran after initial insert/get/compaction implementation, then after
scan/fixed storage and import/measurement work. The final check covers:

- Empty files/scans; unequal lengths; exact-fit maximum 4082; near-full pages;
  oversized/zero rejection without changes; invalid/deleted RIDs; too-small
  outputs without overwrite and scan retries.
- First/middle/last deletion, survivor bytes and RIDs after compaction, deleted
  slot reuse, 100 delete/insert cycles and retained empty-page reuse.
- Multi-page records, close/reopen persistence, exact scan membership/bytes,
  empty pages before/after live pages and repeated EOF.
- Both LRU/MRU and capacities 1/3. Successful closes and continued one-frame
  operations detect leaked pins.
- Four 2500-operation deterministic random tests (seed 20261104), reference map
  by live RID, binary contents, periodic complete scans and close/reopen.
- Fixed-size maximum fit, oversize/zero rejection, padding/state bytes, wrong-L
  and wrong-format opening, multi-page scans and accounting reconciliation.
- Injected PF read/write failures, failed-open cleanup, dirty-data retry and
  scan continuation after a read failure.
- Text parser: LF/CRLF, blank/header modes, 10000-byte untruncated line and
  final unterminated line. Sixteen empty/text/binary imports across both policies
  and formats, plus two oversized imports rejected before file creation.

The complete RM check was rerun for the real-data completion: 18 test executable
runs (16 storage scenarios, parser and fault test), plus 18 importer process
checks, pass. See `experiments/results/phase2-real-check.log`. All four real-data
runs also pass full record/scan validation and accounting checks; there is no
remaining Phase 2 blocker. The earlier Phase 1 check and AM build logs remain
preserved. No shared PF code changed in this data/config/documentation update,
so Phase 1 was not rerun for it. AM runtime correctness is not claimed.

Limits: deleted RIDs may be reused; records are single-page; directories retain
historical slots; first-fit is linear in data pages per insertion; scans do not
support concurrent mutation; fixed baseline is insertion-only. The importer
keeps input/reference bytes in memory. No transaction/crash atomicity, WAL,
concurrency, fsync durability or arbitrary-corruption recovery is provided.
Basic format/bounds validation is not a database repair tool. Errors after an
I/O failure may require retry; do not interpret them as transactional rollback.

Pre-existing PF warnings about old declarations/pointer formatting remain.
The new benchmark includes legacy pftypes.h for exact PF file accounting, which
also exposes its implicit-int warnings. Phase 3 now distinguishes AM_NODE_SIZE
1020 from PF's 4096-byte payload without changing either layout, repairs AM scan
ownership and verifies runtime/index behavior. See the separate Phase 3 guide;
a build alone does not prove runtime correctness.

## File guide and viva

All Phase 2 files are new:

| File | Purpose |
|---|---|
| rm.h, rm.c | Public RID/scan/space APIs and slotted storage |
| internal.h, common.c | Small serialization, error, release and open/cursor helpers |
| fixed.h, fixed.c | Separate fixed-size baseline |
| student.h, student.c | Untruncated byte-preserving source-line loader |
| benchmark.c | Import, measured insertion/scan, validation and byte accounting |
| Makefile, check.sh | GNU89 build and isolated source-based checks |
| test_rm.c | Initial staged insertion/get/compaction/persistence checks |
| test_records.c | Boundaries, fixed baseline and seeded reference-map testing |
| test_failures.c | Test-only PF I/O failure injection |
| test_student.c, test_import.py | Parser and importer edge cases |
| experiments/run.py, config.json | Configurable comparison and missing-data handling |
| experiments/make_synthetic.py, synthetic.txt, synthetic-config.json | Explicit synthetic validation fixture/config |
| experiments/synthetic-results/* | Actual synthetic CSV/table/config/input inspection/build log |
| experiments/results/* | Real student CSV/table/config/inspection/verification, build diagnostics, status and check logs |
| README.md | Layout, APIs, reproduction, results and viva explanation |

Eight likely viva questions:

1. Why use slots? Record bytes can move while the slot number used by an index
   remains stable.
2. What is a RID? A zero-based `(PF page number, slot number)` pair.
3. What happens when a record is deleted? Its slot is cleared and survivors
   are repacked; their slot numbers do not change.
4. Why use a temporary page during compaction? It prevents overlapping copies
   from overwriting source bytes still needed by another survivor.
5. Why copy GetRecord output? PF may evict an unfixed frame; callers must own
   their returned bytes.
6. How does insertion choose space? First-fit over existing pages, accounting
   for a new slot only when no deleted slot can be reused.
7. What does utilization measure? Live record bytes divided by allocated
   PF data-page payload capacity; metadata/padding/unused and PF overhead are
   also reported separately.
8. Why can fixed-size storage waste space? Each live record reserves L bytes
   regardless of its actual length; the difference is padding.

Phase 3 now uses the explicitly accepted field-2 assumption, preserves exact
character keys and duplicate RIDs, and implements/measures all three construction
methods, including direct bulk loading. The authoritative schema is still not
provided. This guide covers the record phase; see [Phase 3](../indexing/README.md)
for the completed index work and retained 1020-byte AM node layout.
