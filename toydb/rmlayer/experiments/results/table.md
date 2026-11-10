> Historical submission measurement; not the new public sample.
> See the root README for data provenance.

# Measured comparison: supplied student data (title excluded)

Utilization = live payload / (data pages × 4096). Metadata pages = 0.

| Format | L | Rep | Records | Payload B | Pages | File B | Metadata B | Padding B | Unused B | PF overhead B | Utilization | Insert ms | Scan ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| slotted | 0 | 1 | 17813 | 1759011 | 462 | 1894208 | 110574 | 0 | 22767 | 1856 | 92.95% | 2714.183 | 3.783 |
| fixed | 108 | 1 | 17813 | 1759011 | 495 | 2029508 | 75212 | 164793 | 28504 | 1988 | 86.76% | 2458.410 | 3.267 |
| fixed | 162 | 1 | 17813 | 1759011 | 743 | 3046308 | 77196 | 1126695 | 80426 | 2980 | 57.80% | 3524.109 | 2.573 |
| fixed | 216 | 1 | 17813 | 1759011 | 990 | 4059008 | 79172 | 2088597 | 128260 | 3968 | 43.38% | 4805.040 | 2.661 |

| Format | L | Rep | Insert logical | Insert reads | Insert writes | Scan logical | Scan reads | Scan writes |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| slotted | 0 | 1 | 4093807 | 4093573 | 17699 | 18277 | 462 | 0 |
| fixed | 108 | 1 | 4415400 | 4415184 | 17708 | 18311 | 495 | 0 |
| fixed | 162 | 1 | 6618644 | 6618500 | 17744 | 18559 | 743 | 0 |
| fixed | 216 | 1 | 8821890 | 8821782 | 17762 | 18806 | 990 | 0 |

Each row uses a fresh database/process and validates all bytes and RIDs after measurement.
Insertion includes final close/flush. Scan includes reopen, cursor setup, iteration and close.
The OS cache is not cleared. Millisecond timings are observations, not reliable speedup claims.
