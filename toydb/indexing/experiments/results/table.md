> Historical submission measurement; not the new public sample.
> See the root README for data provenance.

# Phase 3 measured comparisons

20 PF frames, LRU, 3 repetitions; timings are medians.

| Method | End-to-end s | Existing-file index total s | Sorted construction + flush s | Pages | File bytes | Height |
|---|---:|---:|---:|---:|---:|---:|
| existing | 3.208540 | 0.011344561 | — | 467 | 1914708 | 3 |
| incremental | 3.144835 | — | — | 467 | 1914708 | 3 |
| bulk | 3.073633 | 0.008962791 | 0.00107149 | 293 | 1201308 | 3 |

A/C existing-file totals include extraction, optional sorting, index construction and flush.
B end-to-end includes interleaved record-plus-index creation; it has no separately measured index-only build.
All end-to-end totals include student-file creation. Raw stage timings/counters are retained.

| Method | Stage | Median ms | Logical requests | Physical reads | Physical writes | Results |
|---|---|---:|---:|---:|---:|---:|
| existing | record_create | 3197.415 | 4093807 | 4085654 | 17056 | 17813 |
| existing | extract | 5.665 | 18277 | 462 | 0 | 17813 |
| existing | index_construct | 5.666 | 50489 | 296 | 736 | 17813 |
| existing | index_flush | 0.013 | 0 | 0 | 18 | 17813 |
| existing | index_total | 11.345 | 68766 | 758 | 754 | 0 |
| existing | end_to_end | 3208.540 | 4162573 | 4086412 | 17810 | 0 |
| existing | point_index_cold | 30.060 | 107319 | 21974 | 0 | 17813 |
| existing | point_fetch_cold | 52.110 | 125133 | 43215 | 0 | 17813 |
| existing | range_index_cold | 10.732 | 62667 | 1582 | 0 | 61072 |
| existing | range_fetch_cold | 31.868 | 123740 | 5254 | 0 | 61072 |
| incremental | combined_construct | 3144.792 | 4144299 | 4135898 | 35189 | 17813 |
| incremental | combined_flush | 0.008 | 0 | 0 | 2 | 17813 |
| incremental | end_to_end | 3144.835 | 4144299 | 4135898 | 35191 | 0 |
| incremental | point_index_cold | 29.272 | 107319 | 21974 | 0 | 17813 |
| incremental | point_fetch_cold | 50.272 | 125133 | 43215 | 0 | 17813 |
| incremental | range_index_cold | 10.923 | 62667 | 1582 | 0 | 61072 |
| incremental | range_fetch_cold | 29.902 | 123740 | 5254 | 0 | 61072 |
| bulk | record_create | 3064.771 | 4093807 | 4085654 | 17056 | 17813 |
| bulk | extract | 6.317 | 18277 | 462 | 0 | 17813 |
| bulk | sort | 1.810 | 0 | 0 | 0 | 17813 |
| bulk | index_construct | 1.059 | 294 | 290 | 562 | 17813 |
| bulk | index_flush | 0.012 | 0 | 0 | 20 | 17813 |
| bulk | sorted_construct_total | 1.071 | 294 | 290 | 582 | 0 |
| bulk | index_total | 8.963 | 18571 | 752 | 582 | 0 |
| bulk | end_to_end | 3073.633 | 4112378 | 4086406 | 17638 | 0 |
| bulk | point_index_cold | 27.915 | 107150 | 17345 | 0 | 17813 |
| bulk | point_fetch_cold | 48.891 | 124964 | 37308 | 0 | 17813 |
| bulk | range_index_cold | 10.878 | 62086 | 1000 | 0 | 61072 |
| bulk | range_fetch_cold | 32.247 | 123159 | 4489 | 0 | 61072 |

Point batches query every distinct key. Fetch batches also resolve each encoded RID through RM and compare full bytes.
Range batches use deterministic existing bounds and operators <, >, <=, >=, !=, < (default six queries).
Each query stage reopens files after closing the previous stage, starting with an empty toyDB cache. The OS cache is not cleared.
Index stages use index-only traffic; fetch and end-to-end stages include both student and index traffic. See raw CSV traffic column.
Result sets, complete scans, node structure and all stored source bytes were verified; no key normalization is performed.
