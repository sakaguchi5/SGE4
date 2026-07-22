# Spiral 4 CU1 evidence ledger

| Evidence | Authority | Expected result |
|---|---|---|
| Owner selection record | `NEXT_CAPABILITY_SELECTION_FROM_SPIRAL3.md` | Spiral 4 selected without rewriting Spiral 3 evidence |
| Contract manifest | `CONTRACT_MANIFEST_V1.json` | exact baseline, dimensions, Candidates, corpus, and policies |
| Completion specification | `SGE4-5_Spiral4_Completion_Spec_v0.1.md` | fixed/variable boundary and completion gates complete |
| Baseline schema inventory | current `D3D12Schema.h` | IndirectArgument state present; indirect Operation and Command Signature absent |
| Non-goals | `NON_GOALS_V1.md` | no graph mutation, compaction, temporal, texture, variant, or multi-device scope |
| Project boundary | `PROJECT_BOUNDARIES_V1.md` | no CU1 C++/ABI/Solution change |
| Corpus | `CORPUS_V1.md` | zero, group boundaries, Batch boundaries, and full capacity included |
| Proof ledger | `PROOF_LEDGER_V1.md` | S4-I01–S4-I20 assigned to a first owning CU |
| Source inventory | `SOURCE_MANIFEST.sha256` | regenerated and verified after copying |
| CU1 verifier | `Verify-Spiral4CU1.ps1` | all contract tokens and baseline inventory pass |

## CU1 completion output

```text
SGE4-5 SPIRAL 4 COMPLETION UNIT 1 PASSED
Baseline: 8c1125394ba4b45d571d7cba4e7ad685bb90918b
Research contract: dynamic Active Work Count / verified indirect dispatch / Batch lowering
CU1 implementation: documents and verification only; no C++ or ABI mutation
```
