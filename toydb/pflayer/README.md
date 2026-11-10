> Historical submission document. Supplied-data measurements describe the original
> private run; the public example uses synthetic data. See the root README and
> REDISTRIBUTION.md for current commands, provenance, and permission status.

# Phase 1: page buffering

Phase 1 is implemented on top of milestone 1. Runtime capacity remains intact;
this phase adds per-open-file LRU/MRU selection, explicit dirty marking,
statistics, correctness tests, reproducible workloads and measured graphs.
No slotted pages, record storage or index bulk loading are implemented.

## Architecture and lifecycle

There is one shared buffer pool, one hash table keyed by `(fd, page number)`,
one doubly linked recency list, and a free-frame list. There are no per-file
pools. `PFbufGet` finds existing pages through the hash table. On a miss,
`PFbufInternalAlloc` first takes a free frame, otherwise allocates up to the
configured capacity, otherwise chooses a replacement victim. `PFbufAlloc`
uses the same allocator when appending a new disk page.

Each open-file table entry stores its policy. The requesting file's policy
is passed explicitly through `PFbufGet` or `PFbufAlloc` to the allocator.
It chooses among **all** unfixed frames, even frames belonging to other files:

- `PF_LRU`: scan from the tail through `prevpage`.
- `PF_MRU`: scan from the head through `nextpage`.
- Skip fixed frames; return `PFE_NOBUF` if none are eligible.
- Write a dirty victim with the victim's own descriptor and page number.
- Only after successful write-back clear dirty state and remove the old
  hash/list entry. Failed writes leave the victim present and dirty.

The head is most recent. Successful gets (including clean hits), new frame
allocation, unfixing and explicit dirty marking move the frame to the head.
A rejected get of an already-fixed page does not change recency. Reads do
not mark pages dirty. Unfixing with FALSE never clears a dirty flag.

Close writes remaining dirty frames and puts released frames on the free
list. Frames remain allocated. `PF_Init()` is for startup **once**: it clears
hash/file tables but does not reset the pool, release frames or close files.
Do not call it to reset a workload. Close/reopen files instead.

## Public APIs

Both `pflayer/pf.h` and the AM copy expose the additions:

```c
int PF_SetBufferSize(int num_pages);
int PF_OpenFile(char *filename);                 /* existing API: LRU */
int PF_OpenFileWithPolicy(char *filename, int policy);
int PF_MarkDirty(int fd, int pagenum);
void PF_ResetStats(void);
PF_Stats PF_GetStats(void);                      /* snapshot by value */
```

Example (check every PF result in application code):

```c
PF_Init();
if (PF_SetBufferSize(2) != PFE_OK)
    PF_PrintError("capacity");
fd = PF_OpenFileWithPolicy("data.pf", PF_MRU);
if (fd >= 0 && PF_GetThisPage(fd, 0, &page) == PFE_OK) {
    page[0] = 'X';
    if (PF_MarkDirty(fd, 0) != PFE_OK)
        PF_PrintError("dirty");
    if (PF_UnfixPage(fd, 0, FALSE) != PFE_OK)
        PF_PrintError("unfix");
}
```

Capacity defaults to 20. Positive settings may change repeatedly before
allocation starts. Zero/negative sizes give `PFE_INVALIDBUFSIZE` (-20).
After the first frame allocation attempt, settings give `PFE_BUFALLOCATED`
(-21); even allocation failure or subsequently closing all files does not
unlock configuration. Invalid/late calls leave capacity unchanged.
`PF_MAX_BUFS` remains the default constant, not the runtime limit.

`PF_OpenFileWithPolicy` accepts `PF_LRU` (0) or `PF_MRU` (1), returning a
nonnegative PF descriptor. Other values give `PFE_INVALIDPOLICY` (-22),
before opening a Unix descriptor or consuming a file-table slot.

`PF_MarkDirty` checks the descriptor and page range, then reuses `PFbufUsed`.
The page must be cached and fixed: errors are `PFE_FD`, `PFE_INVALIDPAGE`,
`PFE_PAGENOTINBUF`, or `PFE_PAGEUNFIXED`. `PF_UnfixPage(fd,page,TRUE)` still
marks dirty. Failures set `PFerrno`; successful operations do not clear an
earlier error, following the existing convention. All new errors are printable.

One necessary allocation correction is included: a recycled disposed page
must be marked dirty when its `nextfree` marker changes to `PF_PAGE_USED`.
Dirty marking now covers both append and recycle paths, even when the caller
unfixes with FALSE. A dedicated persistence test covers this.

## Counter definitions

`PF_Stats` uses `unsigned long long` fields:

| Field | Definition |
|---|---|
| `logical_requests` | Each entry into the existing-page lookup path `PFbufGet`. |
| `cache_hits` | That lookup finds `(fd,page)` in the hash table. |
| `cache_misses` | That lookup does not find it. |
| `physical_reads` | A full successful data-page read in `PFreadfcn`. |
| `physical_writes` | A full successful data-page write in `PFwritefcn`, including eviction and close. |
| `page_allocations` | Successful `PF_AllocPage` calls, including recycled pages. |

Hits + misses always equals logical requests. A hit on an already-fixed page
still counts, even though it returns `PFE_PAGEFIXED`. A miss that cannot get
a frame or encounters I/O failure also counts. Invalid descriptor/range
requests rejected before lookup do not count. An in-range disposed page
may enter lookup before being rejected and therefore counts.

`PF_GetFirstPage` calls `PF_GetNextPage`; only actual lookups count. A scan
that skips disposed pages can perform multiple logical page lookups for one
public call. Appending a page is an allocation, not a lookup or disk read.
Recycling a disposed disk page uses the lookup path and also counts as an
allocation. Dirty marking, unfixing and hash-table maintenance do not count
as logical requests.

File-header I/O is excluded. Short/failed data-page operations do not increment
physical counters. These are toyDB file-layer I/O counts, **not measured SSD
accesses**: the operating system may cache reads and writes. This is not an
fsync/crash-durability implementation.

`PF_ResetStats()` only zeroes counters: it does not flush, clear, reorder or
resize the pool. The workload separately counts completed reads and updates.
Several updates can produce one later write-back; physical writes are not
write-query counts.

## Build and correctness tests

From the repository root:

```sh
make -C pflayer check
# Equivalent:
sh pflayer/check.sh
```

The script prints a `/tmp/toydb-check.*` directory containing build logs and
copies of sources. It runs `make -B ... tests` and rebuilds AM's `amlayer.o`.
All test programs run in that temporary directory, with separate processes
for scenarios that need fresh pool state. Bundled objects, executables and
sample files are neither used nor overwritten. Fixtures are created with
stdio so they cannot accidentally allocate buffer frames before configuration.

Checks:

- Supplied `testpf` and `testhash` regression programs.
- Five preserved capacity scenarios: default 20, 2, 1, 25, and allocation.
- `replacement` under LRU, MRU and the default wrapper: A,B,A,C at capacity 2;
  cache hits and physical-read deltas establish the actual victim.
- `allocation` and `reuse` under both policies: append/recycled-page replacement
  obeys policy, counts allocation correctly, and recycled pages remain used
  after close/reopen without an extra caller dirty mark.
- `fixed`: skips fixed candidates, counts failed lookups, rejects all-fixed
  requests, and proceeds after unfixing.
- `dirty`: validates dirty-mark errors, retains dirty state through unfix FALSE,
  checks disk bytes before close, clean eviction produces no writes, legacy
  unfix TRUE works, and close flushes remaining modifications.
- `mixed`: overlapping page numbers in two files, requester-policy victim
  selection, hash isolation and write-back to the victim's own file.
- `stats`: exact hit/miss/read/write/allocation totals, reset preserves cache
  and recency, and GetFirstPage does not double-count through GetNextPage.
- `invalid`: repeated bad policies followed by filling every open-file slot.
- `testiofail`: test-only GNU linker wrapping of read/write simulates a short
  read and failed eviction/close writes. Failed operations do not increment
  physical counts; retry succeeds with the dirty data intact. No production
  inspection API or fault hook was added.

Final validation: 23 test process runs passed (2 supplied, 5 capacity,
15 policy/statistics scenarios, 1 I/O-failure scenario). AM rebuilt successfully.
See `experiments/results/validation.log` for the final recorded output.

## Experiments and graphs

Run experiments using Python's standard library, GNU make and a C compiler:

```sh
python3 pflayer/experiments/run.py
# Alternative configuration/output:
python3 pflayer/experiments/run.py --config my-config.json --output /tmp/my-results
```

Plot with matplotlib (the isolated environment used for this run is below):

```sh
python3 -m venv /tmp/toydb-plot-venv
/tmp/toydb-plot-venv/bin/python -m pip install matplotlib
MPLCONFIGDIR=/tmp/toydb-matplotlib /tmp/toydb-plot-venv/bin/python pflayer/experiments/plot.py
# Alternative results:
MPLCONFIGDIR=/tmp/toydb-matplotlib /tmp/toydb-plot-venv/bin/python pflayer/experiments/plot.py --results /tmp/my-results
```

The runner rebuilds `workload` from copied sources in a temporary directory.
Every configuration/policy gets a fresh process and database fixture. The C
driver sets capacity, populates pages, closes/reopens to empty the toyDB cache,
then resets counters. It reads/updates existing pages, releasing each before
the next operation. It closes before taking the measured statistics snapshot,
then reopens and verifies every value against an in-memory reference. The
snapshot excludes population and verification. Read results are also checked
during the run. Any failure stops the experiment instead of producing a row.

Configuration is JSON (`experiments/config.json`):

| Key | Default | Meaning |
|---|---|---|
| `buffer_sizes` | `[4,16]` | Positive capacities, each smaller than the database. |
| `page_count` | `64` | Number of existing disk pages. |
| `operation_count` | `4000` | Operations per measured run. |
| `seed` | `20261104` | Page-sequence random seed; mix shuffle uses seed + 1. |
| `write_percentages` | `[0,25,50,75,100]` | Must produce integer counts with operation_count. |
| `access_patterns` | `["sequential","hot_random"]` | Sequential cycles or hot-subset random accesses. |
| `hot_pages` | `8` | First eight pages form the hot subset. |
| `hot_percent` | `80` | Probability of choosing the hot subset; otherwise choose a cold page. |

Page sequences are independent of the mix and policy. Update positions are
selected from a seeded shuffled operation-index list, giving exact requested
counts. LRU/MRU receive identical traces, recorded by SHA-256 in each CSV row.
Hot-subset selection is probabilistic; read/write counts are exact. Python's
seeded `random.Random` generates traces; retain the Python version and trace
hashes when comparing reproductions across environments. This run used Python
3.14 and matplotlib 3.11.2.

Outputs in `experiments/results/`:

- `results.csv`: 40 measured rows, configuration, completed counts, PF counters,
  read checksum and trace hash.
- `config.json`: configuration snapshot; `build.log`: source-build diagnostics.
- `summary.md`: measured counter table and completed operation counts.
- `sequential.png` / `.svg`: four panels for sequential scans.
- `hot_random.png` / `.svg`: four panels for the reused-subset workload.
- `completed_operations.png` / `.svg`: actual completed read/update counts.

Each workload graph plots physical reads, physical writes, logical requests
and cache hit rate against write percentage. Color distinguishes capacity;
solid circles are LRU and dashed crosses are MRU. Overlapping lines are
intentional when counters are equal.

## Measured results and interpretation

All 40 runs passed value validation; each completed 4000 operations.
Read/update counts at write percentages 0/25/50/75/100 are respectively
4000/0, 3000/1000, 2000/2000, 1000/3000 and 0/4000.

| Pattern | Frames | LRU reads / hit rate | MRU reads / hit rate |
|---|---:|---:|---:|
| Sequential | 4 | 4000 / 0% | 3811 / 4.725% |
| Sequential | 16 | 4000 / 0% | 3055 / 23.625% |
| Hot random | 4 | 2737 / 31.575% | 3526 / 11.850% |
| Hot random | 16 | 721 / 81.975% | 2803 / 29.925% |

These read/hit values are unchanged across mixtures: changing operation labels
alone does not alter page order. With immediate unfix after every operation,
extra dirty marking also leaves the final recency order the same.

For a sequential cycle larger than the pool, LRU discards each page before
its next visit. MRU retains some older pages across scans, reducing reads.
For the hot subset, LRU benefits from recent reuse; MRU tends to discard
recently useful pages. This experiment does not establish a universal winner.

At 50% updates and 16 frames, sequential LRU/MRU write 2000/1586 pages;
hot-random LRU/MRU write 402/1650 pages. All four runs completed 2000 updates.
Caching coalesces repeated updates until eviction/close; this explains the
difference between update count and write count. At 0% writes, every run
has zero physical writes. The full five-mixture write totals are in summary.md.

## Changes and viva notes

Changed since milestone 1:

| File | Purpose |
|---|---|
| `buf.c` | Explicit policy propagation, two victim directions, successful-get recency and lookup counters. |
| `pf.c` | Policy-open wrapper, validation, dirty API, statistics storage/reset/get, disk counters and recycled-page dirty correction. |
| `pf.h`, `../amlayer/pf.h` | Matching policy/API/error/statistics declarations; include guards for the new structure. |
| `pftypes.h` | Per-file policy, shared internal statistics declaration and corrected internal buffer declarations. |
| `Makefile`, `check.sh` | Build/run new tests and the workload from source. |
| `testutil.h` | Small fixture, assertion and counter-check helpers. |
| `testphase1.c`, `testiofail.c` | Deterministic public-API correctness and failure/retry tests. |
| `workload.c` | Measured single-run page workload and reference validation. |
| `experiments/config.json`, `run.py`, `plot.py` | Editable experiment settings, paired runs and plotting. |
| `experiments/results/*` | Actual configuration, CSV, logs, tables and graphs. |
| `README.md` | Reproduction instructions and explanation. |

Existing capacity code/tests, hash implementation and AM implementation were
preserved. The AM Makefile and successful returns in the supplied tests retain
the compatibility fixes already made in milestone 1.

Before, victim search always started at `PFlastbpage`. Now:

```c
for (tbpage=(policy == PF_MRU ? PFfirstbpage : PFlastbpage);
     tbpage!=NULL;
     tbpage=(policy == PF_MRU ? tbpage->nextpage : tbpage->prevpage)) {
    if (!tbpage->fixed)
        break;
}
```

Before, opening a file had no policy argument. Now the old API delegates:

```c
int PF_OpenFile(char *fname)
{
    return(PF_OpenFileWithPolicy(fname, PF_LRU));
}
```

Trace with two frames (head shown first), unfixing after each access:
A -> `[A]`; B -> `[B,A]`; A -> `[A,B]`. C evicts B under LRU, giving
`[C,A]`, or A under MRU, giving `[C,B]`.

Dirty trace: get A, change bytes, mark dirty, unfix FALSE. Keep B fixed and
request C. Both policies skip B and write A using A's descriptor. Only when
that write succeeds is A's frame reused for C. Direct disk inspection before
close and close/reopen checks verify the changed bytes. A write failure
returns an error without removing or cleaning A; a later retry writes it.

Eight viva questions:

1. Why keep one pool? Files share the existing capacity, hash and recency list;
   policy changes victim choice, not ownership of separate caches.
2. Whose policy selects a cross-file victim? The requesting file's policy.
   The victim's descriptor determines where dirty bytes are written.
3. Why can MRU help a scan? Recently scanned pages may not be needed until the
   next cycle; retaining older pages can yield hits across cycles.
4. Can a fixed frame be evicted? No; all-fixed requests return PFE_NOBUF.
5. Does a write query mean a disk write? No; it marks cached data dirty,
   potentially combining many updates into one later write-back.
6. What does reset do? Zeroes counters only; cached pages and recency survive.
7. Why close before collecting experiment results? To include all remaining
   dirty write-back, preventing undercounting buffered updates.
8. Why reject late capacity changes? Allocated/free frames survive file close;
   safe live resizing would need additional pool-management logic.

## Legacy limitations and checks not claimed

GNU89 remains the default for the supplied K&R/implicit-declaration code.
GCC 15.2.0 builds PF tests and AM successfully, with pre-existing warnings
about missing/old declarations and pointer printing. No broad modernization
was attempted. Failure tests require GNU-compatible linker `--wrap` support.

At the Phase 1 measurement checkpoint, AM used a separate PF_PAGE_SIZE=1020
header and only its build was checked. Phase 3 now names that unchanged node
layout AM_NODE_SIZE=1020 and forwards AM's PF header to the authoritative
4096-byte interface. It also adds verified AM runtime/index and sanitizer tests;
see [Phase 3](../indexing/README.md). The Phase 1 implementation and measured
artifacts are preserved; a build alone is not proof of AM runtime correctness.

PF remains single-threaded with global state, native C disk layouts and no
crash recovery. Opening the same file independently for concurrent writes
retains the original coherence limitation. No allocation-failure injection,
large-file stress test or real-device I/O profiling was performed. The
experiments are deterministic synthetic page workloads, not a benchmark of
record/index operations or elapsed storage latency.
