# Spiral 6 CU5 Architecture Complete

CU5 qualifies the complete exact Sparse Work-Set architecture before real-GPU performance measurement.

## Qualified corpus

```text
21 cardinalities
x 4 exact-set pattern constructors
= 84 exact Sparse sets

84 sets
x 3 equivalent Candidate lowerings
= 252 WARP Candidate executions per qualification process
```

The cardinality corpus is:

```text
0,1,2,31,32,63,64,65,127,128,129,
255,256,257,511,512,1024,2048,3072,4095,4096
```

The pattern corpus is:

```text
PrefixControl
UniformStride
BlockClusteredPermutation
HashScatterPermutation
```

## Required observations

Every Candidate must prove:

- the exact active write count is `K`,
- every active output matches the independent CPU reference,
- every inactive output record remains byte-identical `float4(0,0,0,0)`,
- every active homogeneous coordinate is bit-identical `1.0f`,
- all values are finite,
- the observed Dispatch dimension equals the Verified Plan,
- all 65,536 output bytes are pairwise identical across A/B/C.

At `K=0`, Dense Mask executes its fixed 64 groups but writes nothing. Compact Index List and Active Block List execute zero groups. All three outputs remain the full inactive sentinel Buffer.

## Recovery

Controlled Recovery destroys the owned WARP Device and all current-epoch Sparse execution objects, increments the device epoch, and rematerializes a clean execution context.

The old epoch handle, completion token and Derived Representation handle are rejected. The new epoch requires:

```text
explicit Exact Sparse Set rebind
  -> explicit Dense/Compact/Block representation rebuild
  -> submission
```

Rebinding without rebuilding is rejected with `sparse-runtime/rebuild-required`.

Actual Recovery invokes `ID3D12Device5::RemoveDevice`, moves the Runtime to `AwaitingAdapter`, releases all native execution objects and excludes the removed WARP LUID from retry.

## Completion declaration

A successful CU5 runner may declare:

```text
SGE4-5 SPIRAL 6 ARCHITECTURE COMPLETE
```

This declaration does not authorize Runtime Candidate selection. Real-GPU measurement and the `(Pattern,K)` Decision Report remain CU6 work.
