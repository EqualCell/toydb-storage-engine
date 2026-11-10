# toyDB code directory

See [the root README](../README.md) for the clean-copy workflow and
[redistribution status](../REDISTRIBUTION.md). From this directory:

```sh
make
make check
make sanitize
```

The supplied data has been replaced by `../data/student.txt`, an invented fixture.
Historical measured results remain in each layer's `experiments/results/`.
New record/index experiments write to `experiments/public-results/`.
