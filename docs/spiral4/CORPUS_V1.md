# Spiral 4 canonical corpus v1

## Fixed capacity

```text
boneCount = 8
maxReusePerBone = 512
Nmax = 4096
threadsPerGroup = 64
```

The fixed Work record source is the canonical Spiral 3 R512 point corpus and bone association. Spiral 4 varies only the active prefix length and dynamic Motor values.

## Active Count cases

| ID | Nf | Purpose |
|---|---:|---|
| N00 | 0 | zero-work validity |
| N01 | 1 | minimum nonzero |
| N02 | 63 | one below thread-group boundary |
| N03 | 64 | exact thread group |
| N04 | 65 | one above thread-group boundary |
| N05 | 127 | one below two groups |
| N06 | 128 | exact two groups |
| N07 | 129 | one above two groups |
| N08 | 255 | one below 256 |
| N09 | 256 | exact Batch boundary |
| N10 | 257 | one above 256 |
| N11 | 511 | one below 512 |
| N12 | 512 | exact 512 Batch |
| N13 | 513 | one above 512 |
| N14 | 1023 | one below 1024 |
| N15 | 1024 | exact 1024 Batch |
| N16 | 2048 | half capacity |
| N17 | 4095 | one below capacity |
| N18 | 4096 | full capacity |

## Batch-size candidates

```text
B64
B128
B256
B512
B1024
```

Batch ranges are contiguous and canonical in ascending `firstWork`.

## Dynamic frame sequences

Qualification must include deterministic sequences:

```text
F00 ConstantZero
F01 ConstantOne
F02 ConstantFull
F03 AscendingBoundaryCases
F04 DescendingBoundaryCases
F05 AlternatingZeroFull
F06 Alternating63_65
F07 Alternating127_129
F08 Alternating255_257
F09 Alternating511_513
F10 Alternating1023_1024
F11 FixedSeedMixedCounts
```

Motor values reuse a deterministic finite palette compatible with Spiral 3. Active-count changes and Motor-value changes must not change Frozen bytes.

## Inactive-tail sentinel

Before qualification execution, output records `[0, Nmax)` are initialized to a versioned sentinel.

After execution:

- `[0, Nf)` must equal the reference within the Observation Contract,
- `[Nf, Nmax)` must retain the sentinel byte pattern.

The performance profile records or excludes sentinel-reset cost explicitly; it may not silently mix reset cost into one Candidate only.
