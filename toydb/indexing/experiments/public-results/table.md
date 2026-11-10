# Phase 3 measured comparisons

20 PF frames, LRU, 3 repetitions; timings are medians.

| Method | End-to-end s | Existing-file index total s | Sorted construction + flush s | Pages | File bytes | Height |
|---|---:|---:|---:|---:|---:|---:|
| existing | 0.000639 | 0.000266115 | — | 6 | 24608 | 2 |
| incremental | 0.000515 | — | — | 6 | 24608 | 2 |
| bulk | 0.000615 | 0.000240877 | 4.1428e-05 | 5 | 20508 | 2 |

A/C existing-file totals include extraction, optional sorting, index construction and flush.
B end-to-end includes interleaved record-plus-index creation; it has no separately measured index-only build.
All end-to-end totals include student-file creation. Raw stage timings/counters are retained.

| Method | Stage | Median ms | Logical requests | Physical reads | Physical writes | Results |
|---|---|---:|---:|---:|---:|---:|
| existing | record_create | 0.367 | 546 | 0 | 4 | 240 |
| existing | extract | 0.166 | 246 | 4 | 0 | 240 |
| existing | index_construct | 0.089 | 420 | 1 | 1 | 240 |
| existing | index_flush | 0.013 | 0 | 0 | 6 | 240 |
| existing | index_total | 0.266 | 666 | 5 | 7 | 0 |
| existing | end_to_end | 0.639 | 1212 | 5 | 11 | 0 |
| existing | point_index_cold | 0.160 | 1192 | 6 | 0 | 240 |
| existing | point_fetch_cold | 0.313 | 1433 | 10 | 0 | 240 |
| existing | range_index_cold | 0.172 | 860 | 6 | 0 | 822 |
| existing | range_fetch_cold | 0.664 | 1683 | 10 | 0 | 822 |
| incremental | combined_construct | 0.470 | 966 | 1 | 1 | 240 |
| incremental | combined_flush | 0.020 | 0 | 0 | 10 | 240 |
| incremental | end_to_end | 0.515 | 966 | 1 | 11 | 0 |
| incremental | point_index_cold | 0.161 | 1192 | 6 | 0 | 240 |
| incremental | point_fetch_cold | 0.308 | 1433 | 10 | 0 | 240 |
| incremental | range_index_cold | 0.173 | 860 | 6 | 0 | 822 |
| incremental | range_fetch_cold | 0.658 | 1683 | 10 | 0 | 822 |
| bulk | record_create | 0.369 | 546 | 0 | 4 | 240 |
| bulk | extract | 0.171 | 246 | 4 | 0 | 240 |
| bulk | sort | 0.027 | 0 | 0 | 0 | 240 |
| bulk | index_construct | 0.031 | 6 | 1 | 1 | 240 |
| bulk | index_flush | 0.010 | 0 | 0 | 5 | 240 |
| bulk | sorted_construct_total | 0.041 | 6 | 1 | 6 | 0 |
| bulk | index_total | 0.241 | 252 | 5 | 6 | 0 |
| bulk | end_to_end | 0.615 | 798 | 5 | 10 | 0 |
| bulk | point_index_cold | 0.159 | 1191 | 5 | 0 | 240 |
| bulk | point_fetch_cold | 0.228 | 1432 | 9 | 0 | 240 |
| bulk | range_index_cold | 0.136 | 857 | 5 | 0 | 822 |
| bulk | range_fetch_cold | 0.394 | 1680 | 9 | 0 | 822 |

Point batches query every distinct key. Fetch batches also resolve each encoded RID through RM and compare full bytes.
Range batches use deterministic existing bounds and operators <, >, <=, >=, !=, < (default six queries).
Each query stage reopens files after closing the previous stage, starting with an empty toyDB cache. The OS cache is not cleared.
Index stages use index-only traffic; fetch and end-to-end stages include both student and index traffic. See raw CSV traffic column.
Result sets, complete scans, node structure and all stored source bytes were verified; no key normalization is performed.
