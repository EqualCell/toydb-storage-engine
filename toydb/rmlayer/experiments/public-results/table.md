# Measured comparison: SYNTHETIC public example (240 invented records)

Utilization = live payload / (data pages × 4096). Metadata pages = 0.

| Format | L | Rep | Records | Payload B | Pages | File B | Metadata B | Padding B | Unused B | PF overhead B | Utilization | Insert ms | Scan ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| slotted | 0 | 1 | 240 | 12975 | 4 | 16408 | 1472 | 0 | 1937 | 24 | 79.19% | 0.493 | 0.135 |
| fixed | 77 | 1 | 240 | 12975 | 5 | 20508 | 1000 | 5505 | 1000 | 28 | 63.35% | 0.576 | 0.094 |
| fixed | 115 | 1 | 240 | 12975 | 8 | 32808 | 1024 | 14625 | 4144 | 40 | 39.60% | 0.880 | 0.075 |
| fixed | 153 | 1 | 240 | 12975 | 10 | 41008 | 1040 | 23745 | 3200 | 48 | 31.68% | 1.121 | 0.065 |

| Format | L | Rep | Insert logical | Insert reads | Insert writes | Scan logical | Scan reads | Scan writes |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| slotted | 0 | 1 | 546 | 136 | 39 | 246 | 4 | 0 |
| fixed | 77 | 1 | 695 | 395 | 93 | 248 | 5 | 0 |
| fixed | 115 | 1 | 960 | 756 | 141 | 251 | 8 | 0 |
| fixed | 153 | 1 | 1220 | 1064 | 165 | 253 | 10 | 0 |

Each row uses a fresh database/process and validates all bytes and RIDs after measurement.
Insertion includes final close/flush. Scan includes reopen, cursor setup, iteration and close.
The OS cache is not cleared. Millisecond timings are observations, not reliable speedup claims.
