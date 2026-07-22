# Spiral 6 CU5 evidence ledger

## Bound predecessor

```text
CU3 authority evidence:
C04FCA6674BDA7EA2FBBB9BEE3208757A960EA7B606BE266EA737023573D398C

CU4 accepted commit:
698b86510dda5d11b7259f01ffff288a07076f56
```

CU5 requalifies the complete A/B/C family over the full 84-set corpus; it does not rely on a performance conclusion from CU4.

## Generated evidence

```text
build/tests/spiral6-cu5/architecture-debug-a.bin
build/tests/spiral6-cu5/architecture-debug-b.bin
build/tests/spiral6-cu5/architecture-release.bin

build/tests/spiral6-cu5/controlled-debug-a.bin
build/tests/spiral6-cu5/controlled-debug-b.bin
build/tests/spiral6-cu5/controlled-release.bin

build/tests/spiral6-cu5/fresh-rematerialization-debug-a.bin
build/tests/spiral6-cu5/fresh-rematerialization-debug-b.bin
build/tests/spiral6-cu5/fresh-rematerialization-release.bin
```

Each three-file group must be byte-identical.

## Actual removal evidence

The Debug executable performs an actual `ID3D12Device5::RemoveDevice` qualification. It must prove `Active -> AwaitingAdapter`, stale-handle rejection, native-object release and removed-LUID retry exclusion.

## Completion boundary

Successful execution declares `SGE4-5 SPIRAL 6 ARCHITECTURE COMPLETE`. Runtime Sparse policy remains unauthorized. CU6 owns real-GPU timing and Decision Evidence.
