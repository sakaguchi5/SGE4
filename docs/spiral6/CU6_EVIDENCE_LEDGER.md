# Spiral 6 CU6 evidence ledger

## Bound predecessor evidence

```text
Architecture:
4A634317E988E3F3F9639F249A35AF0CE25E18E19EA52A0500CE75AB09674B7E

Controlled Recovery:
65AEC68060DD68AE42A191E79C5CF2FFFC8538442B8D6C029FE59927709BD77F

Fresh rematerialization:
A9398025D0C5235E2370BFD5D0C34BDFCDC114836A763F8B6151BD95D80FF032
```

## CU6 generated evidence

```text
build/tests/spiral6-cu6/measurement/spiral6_measurement_evidence_v1.bin
build/tests/spiral6-cu6/measurement/spiral6_measurement_samples_v1.csv
build/tests/spiral6-cu6/measurement/spiral6_measurement_summary_v1.json
build/tests/spiral6-cu6/measurement/SPIRAL6_DECISION_EVIDENCE_REPORT.md
```

The binary evidence ends in a SHA-256 checksum and rejects any one-byte corruption.

## Synthetic deterministic evidence

Debug and Release independently generate byte-identical synthetic artifacts proving:

- evidence round-trip,
- trailing-checksum corruption rejection,
- all-six-order positional balance,
- an A/B density crossover,
- B/C locality analysis,
- same-cardinality Pattern-sensitive winner sets,
- Owner-only Decision Report boundaries.

## Real-GPU pass condition

The selected adapter must be a non-software D3D12 adapter. All 44 `(Pattern,K)` cases and all three Candidates must be measured, every numeric/write-set observation must remain valid, all four evidence artifacts must be generated, and Runtime policy authorization must remain `None`.
