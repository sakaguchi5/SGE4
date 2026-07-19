# Spiral 1 Completion Unit 5 Evidence Ledger

- Completion Unit: `CU5 Architecture Final Freeze`
- Owner Stage: `Stage 12 Recovery / Authority / Determinism Freeze`
- Foundation commit: `31c6f41e9b3addff0785196f942cfee639b8acc3`
- Status before execution: `Pending Windows qualification`

## Scope

CU5 adds no new PGA meaning, lowering, Package ABI, Composition capability, or Runtime reselection path. It qualifies the already-frozen CU1-CU4 architecture under reconstruction, loss, corruption and fresh-process boundaries.

## Recovery qualification

`76_Spiral1RecoveryTests` performs:

1. Controlled whole-composition reconstruction for S00, S07, S12 and S15.15.
2. Exact device epoch increment and full four-Leaf/five-Flow rematerialization.
3. Rejection of pre-recovery shared resource and completion-token handles.
4. Byte-identical Matrix, Direct PGA and Comparison readback before/after recovery.
5. Actual `ID3D12Device5::RemoveDevice` on WARP.
6. Removed-LUID exclusion and canonical `AwaitingAdapter` quarantine for the old domain.
7. Rejection of stale handles and submissions on the removed domain.
8. Canonical no-alternate-adapter handling: the removal process remains `AwaitingAdapter` when the excluded WARP LUID is the only eligible adapter.
9. Reconstruction of the same Frozen artifact in a separate fresh process with the same Observation V2 readback before and after the actual-removal process.

## Authority and determinism qualification

`77_Spiral1FreezeTests` rebuilds and independently validates all S00-S15 cases. Its emitted evidence fixes:

- Corpus and Observation V2 identities.
- native Program target profile identity.
- 31 Frozen Comparison identities.
- dual Representation candidate and Package execution identities.
- embedded auxiliary Package execution identities.
- verified four-Leaf/five-Flow Composition shape.

The final mutation suite rejects Matrix payload, Direct PGA payload, verifier seal, candidate identity, Observation identity, Frozen Composition bytes, Source Package bytes and Comparison Package bytes.

## Promotion

CU5 is complete only when Debug process A, Debug process B and Release produce byte-identical architecture, WARP corpus, controlled-recovery, fresh-process rematerialization and final-freeze evidence; the actual RemoveDevice process must independently prove removed-LUID exclusion, `AwaitingAdapter` quarantine and stale-handle/submission rejection.
