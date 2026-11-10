> Historical submission measurement; not the new public sample.
> See the root README for data provenance.

# Measured comparison: SYNTHETIC validation only — student.txt missing

Utilization = live payload / (data pages × 4096). Metadata pages = 0.

| Format | L | Rep | Records | Payload B | Pages | File B | Metadata B | Padding B | Unused B | PF overhead B | Utilization | Insert ms | Scan ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| slotted | 0 | 1 | 240 | 38740 | 10 | 41008 | 1520 | 0 | 700 | 48 | 94.58% | 1.424 | 0.088 |
| fixed | 290 | 1 | 240 | 38740 | 19 | 77908 | 1112 | 30860 | 7112 | 84 | 49.78% | 1.791 | 0.040 |
| fixed | 435 | 1 | 240 | 38740 | 27 | 110708 | 1176 | 65660 | 5016 | 116 | 35.03% | 4.167 | 0.091 |
| fixed | 580 | 1 | 240 | 38740 | 35 | 143508 | 1240 | 100460 | 2920 | 148 | 27.02% | 4.181 | 0.096 |

| Format | L | Rep | Insert logical | Insert reads | Insert writes | Scan logical | Scan reads | Scan writes |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| slotted | 0 | 1 | 1317 | 1179 | 174 | 252 | 10 | 0 |
| fixed | 290 | 1 | 2318 | 2240 | 204 | 262 | 19 | 0 |
| fixed | 435 | 1 | 3294 | 3240 | 216 | 270 | 27 | 0 |
| fixed | 580 | 1 | 4200 | 4158 | 222 | 278 | 35 | 0 |

Each row uses a fresh database/process and validates all bytes and RIDs after measurement.
Insertion includes final close/flush. Scan includes reopen, cursor setup, iteration and close.
The OS cache is not cleared. Millisecond timings are observations, not reliable speedup claims.
