# R3 composition authority ownership V1

| Fact | Authoritative owner | Non-owner behavior |
|---|---|---|
| Leaf executable authority | R2 `OpaqueFrozenArtifactV1` | Composition references identities only |
| Endpoint projection | Leaf-bound Resource-contract projection | Flow author cannot rewrite its owner identity |
| Flow membership and boundary | Raw Composition Contract | Planner may only propose consequences |
| Allocation, schedule, binding, handoff, synchronization | Independent Composition Verifier after reconstruction | Planner proposal is unverified |
| Presenter | Derived from endpoint authority and output-flow membership | Runtime may not choose a different presenter |
| Recovery set | Verified whole-composition set | Partial recovery is not represented |
| Composition seal | Independent Composition Verifier | Planner and Frozen builder cannot mint it |
| Frozen Composition identity | Verified-only Frozen builder | Raw Contract and Planner proposal cannot freeze |

## Single-fact rule

Verified and Frozen composition objects store typed identities and counts rather than independent mutable copies of Semantic, Candidate or leaf-interface descriptions. R2 remains authoritative for every leaf plan; R3 owns only inter-leaf composition facts.
