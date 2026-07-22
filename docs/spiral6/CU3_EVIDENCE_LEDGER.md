# Spiral 6 CU3 evidence ledger

The runner generates deterministic Debug and Release evidence at:

```text
build/tests/spiral6-cu3/debug.bin
build/tests/spiral6-cu3/release.bin
```

The evidence commits:

- the canonical Raw proposal;
- the opaque Verified Plan and Verifier certificate;
- the CU2 Sparse artifact and epoch-bound Resource binding;
- the verified WARP observation;
- the count of 36 rejected Raw proposal attacks.

Debug and Release evidence must be byte-identical. The runner prints the SHA-256 of the retained Debug evidence.
