# Spiral 6 canonical sparse corpus V1

## Universe

```text
WorkUniverseCount = 4096
ThreadsPerGroup = 64
BlockCount = 64
BlockSize = 64
```

The Semantic set is encoded as strictly increasing unique `uint32` universe indices.

## Qualification cardinalities

```text
0, 1, 2, 31, 32, 63, 64, 65, 127, 128, 129,
255, 256, 257, 511, 512, 1024, 2048, 3072, 4095, 4096
```

These cover zero work, one work item, warp/group boundaries, block boundaries, intermediate densities and complete density.

## Measurement cardinalities

```text
1, 8, 32, 64, 128, 256, 512, 1024, 2048, 3072, 4096
```

## Pattern constructors

### PrefixControl

`0..K-1`.

### UniformStride

For `K>0`, index `j` is `floor(j*4096/K)`.

### BlockClusteredPermutation

Block order is `(17*j+5) mod 64`. Fill complete blocks and then the lowest lanes of the next block.

### HashScatterPermutation

Take the first `K` values of `(4051*j+17) mod 4096`, then sort.

## Required corpus checks

- exactly `K` unique indices,
- all indices in range,
- canonical increasing bytes deterministic,
- derived dense mask popcount equals `K`,
- compact list equals canonical set bytes,
- block-list local-mask popcount equals `K`,
- all three derived representations reconstruct the same set.
