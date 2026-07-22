# Spiral 6 CU4 evidence ledger

## Bound predecessor

```text
CU3 authority evidence:
C04FCA6674BDA7EA2FBBB9BEE3208757A960EA7B606BE266EA737023573D398C
```

## Generated evidence

The runner writes:

```text
build/tests/spiral6-cu4/debug.bin
build/tests/spiral6-cu4/release.bin
```

The two files must be byte-identical. The retained evidence commits:

- the canonical Sparse Semantic;
- all twelve exact Sparse sets;
- all 36 Raw Candidates;
- all opaque Verified Plans and certificates;
- all Frozen representation artifacts and epoch-bound Resource bindings;
- all WARP observations;
- pairwise 65536-byte Output identity;
- 40 rejected Raw-family mutation attacks;
- cross-set, Resource, role, layout and device-epoch rejection gates.

The runner prints the SHA-256 of `debug.bin` after byte equality is established.
