# CU6 Measurement and Decision Evidence Ledger

## Scope

CU6 contains Stage 13 real GPU measurement and the non-selection portion of Stage 14. It does not choose the next Spiral capability.

## Evidence

| Item | Evidence |
|---|---|
| Hardware adapter | non-software DXGI adapter and fingerprint |
| Fair ordering | ABBA raw sample stream |
| GPU time | D3D12 timestamp query and queue frequency |
| CPU costs | Candidate, Verifier, Freeze, materialization, command recording and end-to-end |
| Resources | Package, constants, upload, allocation and readback bytes |
| Correctness | Observation Contract V2 on every measured case |
| Report | Markdown and JSON Decision Evidence |
| S1-I17 | Qualified by Stage 13 report |
| S1-I18 | Preserved; next capability selection deferred by owner |

## Non-promotion

No benchmark winner becomes Runtime policy. No adaptive selection, variant set or new Level 4 capability is added.
