# Spiral 4 CU3 evidence ledger

| Evidence | Producer | Required result |
|---|---|---|
| Raw proposal bytes | Candidate/Planner | deterministic, self-identified, non-authoritative |
| dependency boundary | CU3 source verifier | Verifier has no Planner reference |
| Verified type boundary | C++ static assertions | no aggregate/default/public Raw construction |
| independent derivation | Verifier | every indirect decision rederived |
| mutation corpus | authority test | 27/27 invalid proposals rejected |
| recomputed-Raw controls | authority test | still rejected by independent derivation |
| certificate context | Verifier | Target/Command Signature replay rejected |
| Freeze context | Verified execution | Observation replay rejected |
| sidecar binding | Verified execution | verified artifact identity equals actual artifact |
| Program binding | Verified execution | template identities included in binding |
| shader evidence | CU2 low-level executor | actual producer/Consumer DXBC digests recorded |
| verified WARP | authority test | Nf 0, 65, 4096 pass |
| deterministic authority bundle | CU3 runner | Debug and Release bytes identical |
| Runtime boundary | all layers | no Runtime dispatch-dimension decision |

## Important distinction

The Debug/Release authority bundle contains Semantic, Raw, Verified, sidecar,
and Frozen binding bytes. It does not include compiled shader bytecode because
Debug and Release compilation flags intentionally produce different DXBC.

The compiled shader digests are runtime evidence bound into the per-run actual
execution identity.
