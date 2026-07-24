# Level 4 v2 R2 evidence ledger V1

| Evidence | Required property |
|---|---|
| Spiral 7 frozen reference gate | Closed reference implementation remains intact |
| R0 regression gate | 40 carried Invariants and 16 frozen sources remain valid |
| R1 regression gate | Strong vocabulary and accepted deterministic Evidence remain valid |
| Project dependency audit | Verifier has no Planner Project dependency; Frozen builder has no Planner dependency |
| Construction-access audit | Verified Plan creation method is private and friends only the independent Verifier |
| Planner/Verifier source audit | Separate operation reconstruction functions exist in separate Projects |
| Valid-chain self-test | Canonical request reaches Verified and Frozen boundaries |
| Mutation/replay matrix | All 18 corrupt or replayed cases reject without a Verified result |
| Debug Evidence | `build/tests/level4v2-r2/authority-evidence-debug.bin` |
| Release Evidence | `build/tests/level4v2-r2/authority-evidence-release.bin` |
| Cross-configuration comparison | Debug and Release Evidence are byte-identical and match the frozen SHA-256 |
| Source Manifest | All applied source, documentation and Solution registration are tracked |

No R2 Evidence authorizes Runtime Candidate selection or executes the Frozen artifact.
