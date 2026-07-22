# Spiral 4 CU6 evidence ledger

| Evidence | Required result |
|---|---|
| CU5 binding | Architecture, Controlled Recovery, and fresh-rematerialization SHA-256 values match the completed CU5 evidence |
| Candidate identity | all seven Raw, Verified, certificate, artifact, and Frozen binding identities recorded |
| hardware identity | adapter metadata, driver, timestamp frequency, LUID, and fingerprint recorded |
| order balance | fourteen canonical orders; every Candidate appears twice in every position |
| measurement corpus | seven Candidates × nineteen Active Counts |
| canonical sample count | 56 measured samples per Candidate and Nf; 7448 total |
| GPU decomposition | argument, state, execution, output synchronization, and observation-copy times recorded |
| CPU decomposition | input preparation, recording, submit/wait, numeric observation, and end-to-end times recorded |
| numeric authority | reference, finite, homogeneous-coordinate, inactive-tail, and Work-structure checks pass for every timed invocation |
| intermediate authority | Single and Batched argument records match the Verified formulas |
| binary integrity | trailing SHA-256, strict parser bounds, and corruption rejection pass |
| summary | per-Nf medians, winner sets, and best Batch size emitted |
| crossover | Fixed/Single and Single/BestBatch classifications emitted without inventing a transition |
| policy boundary | Runtime policy remains None |
| Owner boundary | next capability remains DeferredByOwner / OWNER_DECISION_REQUIRED |

## Authoritative files

`spiral4_measurement_evidence_v1.bin` is the machine-readable authority.

The CSV, JSON summary, and Markdown report are deterministic projections of the
validated binary evidence. They are not accepted as replacements for the binary
file.
