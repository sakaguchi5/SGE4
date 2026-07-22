# Next capability selection from Spiral 5

## Owner decision

The Owner selects:

```text
Exact Sparse Work-Set Flow and Verified Sparse Lowering
```

for Spiral 6.

Baseline commit:

```text
46554ab55e532c438c9c4214ff1df3e7cd68638e
```

## Evidence basis

Spiral 5 completed:

- one exact piecewise-constant Temporal Semantic,
- verified update schedules and source generations,
- A/B/C history-materialization Candidates,
- all nine WARP schedules,
- deterministic architecture and Recovery qualification,
- real-GPU measurement on NVIDIA GeForce RTX 4070 Laptop GPU,
- one stable A/B winner transition between `U=2` and `U=4`,
- no observed B/C winner transition,
- Runtime policy authorization remained `None`.

Formal evidence:

```text
Architecture:
EB4937B1181E3377FD5367D02F218C682AA1F402A5C4099D8B5F452C103D84A1

Controlled Recovery:
6DFBA1F98F1338BE96E7985C7532A20A1B4144B81408BEA0FFD8C03ADCA4DB17

Fresh rematerialization:
6B0D6F9A617D257268C84AFF523FBA6D4F9A727A0EA130B956E7DB992123300B

Measurement evidence:
930ABD5D83A30B31A76551B9D8D35D89D527B7DD7EAC83FA605A94AA252DCF92

Decision report:
8B4A6E532F424CC6525E2FA07A5C054D57343A333C211DC0451556CD4F10DBA5

Measurement summary:
4C22E5D2D9CFC7CEA0038967822CCFBD7558AAA955A7B340A01ADE4591021FF6
```

## Why Sparse follows Temporal

Spiral 4 allowed a variable active quantity only as one contiguous prefix:

```text
[0, Nf)
```

Spiral 5 varied reuse through time while keeping all 4096 Work records active. It explicitly excluded sparse Work selection.

The remaining structural question is therefore not how many prefix records execute and not when a generation changes. It is:

```text
Given one exact subset S of a fixed 4096-record Work universe,
how should membership be materialized and issued without changing S?
```

This is the spatial/set counterpart of Spiral 5's temporal reuse question.

## Selected comparison

```text
A. dense 4096-bit membership mask and full-universe predicate dispatch
B. compact exact index list and K-thread sparse dispatch
C. compact active-block list with one local 64-bit mask per active block
```

All three must implement the same exact set `S`, write exactly the active output indices, and preserve the inactive sentinel bytes.

## Authority boundary

This selection does not authorize SGE to decide:

- game visibility, culling, importance, distance, or LOD,
- which objects or records are active,
- sparse cardinality `K`,
- spatial pattern or locality,
- approximate omission,
- Runtime adaptive Candidate selection.

The exact sparse set is supplied externally and verified. Candidate representations are derived by the Compiler and independently checked by the Verifier. Hardware remains a Target/Measurement fact.

## Closure status

This Owner decision closes Spiral 5's next-capability gate and opens Spiral 6 CU1.

```text
SGE4-5 SPIRAL 5 CLOSED
```

It does not pre-authorize a Spiral 6 ABI extension, Candidate winner, Runtime policy, or universal sparse-performance rule.
