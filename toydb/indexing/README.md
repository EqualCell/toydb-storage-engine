> Historical submission document. Supplied-data measurements describe the original
> private run; the public example uses synthetic data. See the root README and
> REDISTRIBUTION.md for current commands, provenance, and permission status.

# Phase 3: snapshot indexes over slotted student records

The default key is field 2 of the supplied student export, treated as an exact
8-byte character value. This is an explicitly accepted schema assumption, not
an authoritative column definition. The title is descriptive and is excluded.
The actual source has 17,813 records and 17,810 distinct keys, including three
duplicate pairs. Leading zeros, uppercase letters and embedded spaces remain
unchanged. No integer conversion, trimming or implicit record skipping occurs.

## Architecture and APIs

PF pages remain 4096 bytes. AM nodes remain 1020 bytes, now explicitly named
`AM_NODE_SIZE`. AM's local pf.h forwards to the authoritative PF header. Native
AM header layouts remain unchanged (24-byte leaf, 8-byte internal on this ABI).
This clarification does not change physical PF pages or the original AM layout.

`index.h` defines a small record/index adapter:

- IX_Key: find the configured semicolon-delimited column using byte lengths;
  require its exact configured length. Missing/short/long/NUL-containing keys
  fail explicitly (AM's character comparator stops at NUL).
- IX_Encode / IX_Decode: checked `page*1024+slot` and inverse. Validate page>=0,
  0<=slot<1024 and page<=(INT_MAX-slot)/1024 before arithmetic. RM's slot bound
  fits the radix. Only nonnegative references are used.
- IX_BuildExisting: one operation scanning a populated RM file and inserting
  every key/RID with ordinary AM insertion. IX_Collect and IX_InsertPairs expose
  the same preparation/construction steps for separate experiment measurement.
- IX_Incremental: preflight all source keys, insert each RM record, immediately
  insert its key and encoded RID. Optional reference output supports tests.
- IX_Sort: in-memory sorting by the same strncmp order AM uses, then integer
  RID for deterministic ties. Keys are zero-padded beyond configured length.

All storage operations use PF/RM/AM. The reference adapter does not add a cache
or database file format. IX_INVALID, IX_RM, IX_AM and IX_NOMEM are negative
errors; IX_LastError retains the underlying RM/AM error where applicable.
Malformed input fails, rather than silently losing or truncating records.

These indexes represent snapshots. Decode a returned reference, call
RM_GetRecord and compare its extracted key/full bytes with the expected record.
Future deletion or slot reuse requires corresponding index maintenance; a
new general mutation API is outside this phase.

## AM repairs

AM_Search now starts a fresh search path. Insertion retains that path until
split propagation finishes. Read-only scans clear their temporary search path;
AM_CloseIndexScan also clears it. Stack push/pop/top operations check bounds
and propagate AME_STACK. Failed type/length searches release their page pins.

The old scanner also used page pointers after unfixing and had boundary/error
handling that could unfix the wrong next leaf. The scan cursor now retains
copied key/list state, not frame pointers, validates a candidate while fixed,
and releases it before returning. It seeks at open for equality and lower-bound
queries, filters supported predicates, and reacquires the previous key to support
the supplied deletion-during-scan tests. Searches never accumulate stale paths.

GetLeftPageNum follows first-child pointers from root 0 instead of assuming
leftmost page 2. This supports both legacy ordinary-built and bulk-built trees.
The original root split still copies the old root's children to new pages and
rewrites page 0 as an internal node; no separate root catalog is needed.

Root splits obtain required frames before modifying the old node. Insufficient
frames release pins; the one/two-frame root-leaf failure tests preserve the old
root and dispose an unused allocation. Three frames are the supported minimum
for general ordinary construction. This is not transactional rollback of arbitrary
mid-propagation I/O failures: failed construction should be discarded/rebuilt.

The custom bcopy replacement used memcpy even for overlapping insertion shifts;
AM now uses the declared libc bcopy implementation with overlap-safe semantics.
Errors are explicit for a duplicate list that exceeds one node or a full
single-key leaf that the legacy splitter cannot divide safely.

## Direct sorted bulk loading

AM_BulkLoad accepts sorted AM_KeyRID pairs and an empty, initialized AM index.
It does not call AM_InsertEntry. The native node layout is shared by all methods:

- Leaves contain sorted distinct keys and short RID-list head offsets. Each RID
  cell is an int plus short next offset, growing backward from AM_NODE_SIZE.
- A key's whole duplicate list stays on one leaf. With length L the maximum
  list in an otherwise empty leaf is floor((1020-24-L-2)/6), or 164 for L=8.
- Unique-key nominal leaf capacity is floor((1020-24)/(L+8)); 62 for L=8.
- Internal maxKeys is floor((1020-8-4)/(L+4)), rounded down to even; 84 for L=8.
  Layout is header, first child int, then separator/child pairs.
- Each separator is the minimum key in its right child. Non-root internal
  groups have at least maxKeys/2+1 children. The root may have fewer.

The loader plans leaf groups by byte cost, rebalances neighboring ranges when
the last page is short, allocates leaf page numbers, writes their links, then
builds parent levels from child page/minimum-key descriptors. It distributes
children evenly so a final internal node cannot be a singleton/underfilled node.
The final root is written to page 0. Empty input leaves an empty root leaf;
a one-leaf input is written directly to page 0.

For example, 63 distinct 8-byte keys form leaves of 31 and 32 keys. Root 0
contains the first key of the right leaf as separator and two child pointers.
For larger input, each parent groups at most 85 children; another level is
built until the root fits. All leaves have equal depth. No constructed page
remains pinned across another allocation: only bounded descriptor arrays and
sorted input are kept in memory, not copies of all index pages.

Whole duplicate groups can make half-occupancy packing impossible for a
pathological distribution. Such an input is rejected with AME_DUPLIMIT before
page construction, rather than splitting lists ordinary scans cannot follow.
The supplied duplicate pairs are fully supported. This is an in-memory sort,
not an external-memory bulk sorter.

AM_ValidateTree checks node bounds, key ordering, list bounds/nonnegative
references, child validity/acyclicity, internal occupancy, exact separators,
equal leaf depth, complete leaf links, reachability and expected entry count.
Tests separately compare all returned key/RID pairs and full RM bytes.

## Build and tests

From the code repository root:

```sh
make
make check
make sanitize
```

`make` forces source rebuilds. `make check` runs all PF, RM, supplied AM and new
index checks in fresh source-only temporary copies. It does not rely on bundled
objects or earlier /tmp files. `make sanitize` repeats focused Phase 3 tests
with AddressSanitizer and UndefinedBehaviorSanitizer; leak reporting is disabled
because the original PF pool is intentionally process-lifetime storage.

Phase 3 covers all methods on empty/single inputs, capacities 62/63, ascending,
descending/shuffled/space/letter keys, duplicate groups, long-key multi-level
trees, all six scan predicates, absent keys, thousands of repeated searches,
interleaved files/inserts, reopen, overflow and invalid-key checks, buffer failure
cleanup and ordinary insertion/root splitting after bulk loading. Bulk also runs
with one frame; general ordinary builds use three or more. The real-data queries
run in separate fresh processes after construction.

The original AM tests are linked explicitly because their supplied Makefile
lacked test targets. Their complete output sequences are compared with expected
references. Legacy main functions lack explicit return statements and can exit
12 at -O0 or 0 with optimization; output checking is required. Existing legacy
PF/AM warnings remain; the checked paths pass sanitizers. Tests do not establish
crash recovery, arbitrary corruption repair or all possible concurrency behavior.

## Reproducible experiment

```sh
python3 indexing/experiments/run.py
# Separate output for a custom config:
python3 indexing/experiments/run.py --config my-config.json --output /tmp/index-results
```

Configuration: dataset path (relative to config), skip_header, key_column,
key_length, buffer_capacity (>=3 for the full comparison), policy LRU/MRU,
methods, repetitions, seed and range_queries. Defaults are field 2, length 8,
20 frames, LRU, all methods, three repetitions and seed 20261104. Config changes
need no source edits. Supplied paths expect `data/` beside the code directory;
the submission preserves that structure.

All input and key validation occurs before measured construction. Each method
and repetition has fresh files and a fresh process. Every query experiment is
a second process. All source records, key/RID sets, complete scans and tree
structure are verified before a successful row is emitted.

Timing/counter stages:

- A/C record_create: create, populate and close/flush the student file.
- A/C extract: reopen, scan and collect keys/RIDs, then close.
- C sort: in-memory ordering alone.
- A/C index_construct: create/open index and insert or directly bulk-build.
- A/C index_flush: final index close and remaining dirty write-back.
- C sorted_construct_total: construction plus flush on already sorted pairs.
- A/C index_total: extraction + optional sorting + construction/flush, with
  student file already present.
- B combined_construct/flush: interleaved student-plus-index creation and final
  closes. A separate index-only B cost cannot be inferred from shared counters.
- end_to_end: comparable complete student-plus-index creation for every method.
- point_index_cold / point_fetch_cold: every distinct key in seeded shuffled
  order; either index only or index plus full RM fetch/byte comparison.
- range_index_cold / range_fetch_cold: deterministic rank bounds with operators
  <, >, <=, >=, !=, < by default; exact expected membership and counts checked.

Every query stage closes previous files, then resets counters before reopen,
so it begins with an empty toyDB cache. Within a batch the cache naturally warms.
The OS cache is not cleared. Query result comparisons in memory are included in
the query timing; extra source/reference preparation and structural validation
are outside it. All timers use CLOCK_MONOTONIC.

Counters follow Phase 1 definitions. CSV traffic labels distinguish student,
index, none and combined traffic. Preparation/student creation counters must not
be described as index-only. End-to-end totals sum disjoint stage counters;
reported timing medians are computed per recorded stage, not fabricated sums
of unrelated median values. Raw runs remain in results/raw.csv.

## Results and interpretation

See [measured tables](experiments/results/table.md), [raw runs](experiments/results/raw.csv),
[medians](experiments/results/medians.csv) and [comparison graph](experiments/results/comparison.png).
The final project report reads those actual results alongside preserved Phase
1/2 measurements. Bulk loading has a denser index than repeated insertion;
student first-fit population dominates end-to-end time in this configuration.
Small timing differences are not treated as universal speedups.

Optional plotting dependency: matplotlib. From a new environment:

```sh
python3 -m venv /tmp/toydb-plots
/tmp/toydb-plots/bin/python -m pip install matplotlib
MPLCONFIGDIR=/tmp/toydb-mpl /tmp/toydb-plots/bin/python indexing/experiments/plot.py
```

The three layers remain single-threaded, native-format teaching implementations.
Indexes require snapshot-consistent records; deletion/slot reuse must update
index entries. AM's 1020-byte node leaves unused PF payload space deliberately
for layout compatibility. No node-size migration or external sorting is provided.
