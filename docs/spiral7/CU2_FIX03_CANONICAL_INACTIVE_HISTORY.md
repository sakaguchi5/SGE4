# Spiral 7 CU2 FIX03 — Canonical inactive history initialization

## Observed failure

`HoldPrefix64` passed Active, Retain and Transition-audit checks but failed inactive sentinel stability.

## Root cause

`BuildCpuReferenceOutputV1` created the reference/history vector with the default constructor of `PointRecordV1`.
That type has `w = 1.0f` as its default member initializer, so inactive records became
`float4(0,0,0,1)` instead of the frozen inactive sentinel `float4(0,0,0,0)`.
The first invocation wrote correct GPU zeros outside the active set, but the test timeline then stored the incorrect CPU reference bytes as previous history. A zero-transition Hold correctly copied those bytes and exposed the mismatch.

## Correction

Initialize every reference output record explicitly as `PointRecordV1{0,0,0,0}` before writing active records.

This correction changes no Candidate, artifact format, identity field, Dispatch rule, ABI, Runtime or Backend boundary. It repairs the canonical CPU history oracle so inactive history bytes match the contract.
