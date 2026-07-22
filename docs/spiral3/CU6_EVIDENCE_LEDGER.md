# Spiral 3 CU6 evidence ledger

Status before owner execution: implemented; real-hardware evidence is not yet qualified.

| Evidence | Required result |
|---|---|
| Adapter authority | A non-software D3D12 adapter, LUID, driver version, timestamp frequency and fingerprint are recorded. |
| Target authority | Every candidate/source Package has one common Schema 17 / Runtime 17 target-profile identity. |
| Fair ordering | `ABC`, `ACB`, `BAC`, `BCA`, `CAB`, `CBA`; two warmups/order, eight measured cycles/order, two runs. |
| Sample count | 96 measured order samples per R and 576 across R1/R4/R16/R64/R256/R512. |
| Candidate isolation | A, B and C execute in separate verifier-sealed Frozen Compositions; common source cost is recorded separately and excluded from candidate GPU total. |
| Preparation | generation, independent verification, Leaf freeze, Composition freeze and materialization time are preserved. |
| Runtime metrics | per-Leaf command/GPU time, dispatch/barrier count, candidate GPU total and end-to-end time are preserved. |
| Structural metrics | candidate/source Package bytes, Frozen Composition bytes, dynamic bytes, point bytes, logical intermediate bytes and Leaf counts are preserved. |
| Numerical diagnostics | maximum absolute, relative and ULP distance are recorded; they are not used to silently change the Observation Contract. |
| Crossover | winner per sampled R and adjacent observed transition intervals are reported without interpolation or global representation claim. |
| Authority boundary | `RecommendationAuthority = NonAuthoritative`, `RuntimePolicyAuthorization = None`. |
| Owner boundary | `NextCapabilitySelection = DeferredByOwner`, `SelectionStatus = OWNER_DECISION_REQUIRED`. |

Windows completion command: `run_sge4_5_spiral3_cu6_measurement_decision_evidence.bat`.
