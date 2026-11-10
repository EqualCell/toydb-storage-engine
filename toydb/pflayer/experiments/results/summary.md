> Historical submission measurement; not the new public sample.
> See the root README for data provenance.

# Measured results

All runs validated final page values against an in-memory reference.

| Write % | Completed reads | Completed updates |
|---:|---:|---:|
| 0 | 4000 | 0 |
| 25 | 3000 | 1000 |
| 50 | 2000 | 2000 |
| 75 | 1000 | 3000 |
| 100 | 0 | 4000 |

Physical reads and hit rates were unchanged across mixtures for each pattern/capacity/policy.

| Pattern | Frames | Policy | Physical reads | Hit rate | Writes at 0 / 25 / 50 / 75 / 100% |
|---|---:|---|---:|---:|---|
| hot_random | 4 | LRU | 2737 | 31.575% | 0 / 891 / 1607 / 2224 / 2737 |
| hot_random | 4 | MRU | 3526 | 11.850% | 0 / 960 / 1880 / 2723 / 3526 |
| hot_random | 16 | LRU | 721 | 81.975% | 0 / 232 / 402 / 557 / 721 |
| hot_random | 16 | MRU | 2803 | 29.925% | 0 / 900 / 1650 / 2266 / 2803 |
| sequential | 4 | LRU | 4000 | 0.000% | 0 / 1000 / 2000 / 3000 / 4000 |
| sequential | 4 | MRU | 3811 | 4.725% | 0 / 977 / 1943 / 2881 / 3811 |
| sequential | 16 | LRU | 4000 | 0.000% | 0 / 1000 / 2000 / 3000 / 4000 |
| sequential | 16 | MRU | 3055 | 23.625% | 0 / 821 / 1586 / 2323 / 3055 |

*Mixtures in this run: 0, 25, 50, 75, 100%.

Logical requests equal the configured operation count in every run. Header I/O and validation are excluded; close-time writes are included.

Overlapping graph lines are intentional: page sequences do not change with read/write labels.
