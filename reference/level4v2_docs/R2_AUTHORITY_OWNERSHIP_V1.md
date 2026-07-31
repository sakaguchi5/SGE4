# Level 4 v2 R2 authority ownership V1

| Layer | Authoritative fact | Non-authoritative data it may carry |
|---|---|---|
| Canonical definitions | Semantic, Invocation, Target, resource-contract and legal write-set material and identities | none |
| Candidate Planner | Proposal construction | claimed identity bindings and proposed operation sequence |
| Independent Verifier | acceptance, expected operation reconstruction, Verified Plan identity and seal | Planner proposal as comparison input |
| Verified authority | opaque Verified Plan and identity references sealed by the Verifier | no duplicated Semantic or Invocation description |
| Frozen builder | deterministic Frozen identity derived from Verified authority | no Raw Candidate, no live handle, no execution policy |

## Single-fact rule

The Planner proposal is not a second authority source. Its copies are claims that must equal independently recomputed Canonical identities.

Verified and Frozen objects retain typed identity references. They do not serialize independent mutable copies of Semantic descriptions, Invocation dimensions or Planner operations.

R3 may reference the Verified Plan identity, but must not reopen or reinterpret these bindings.

A directly aggregate-constructed R1 `FrozenArtifactDescriptorV1` is only a descriptor value. R2 authority is represented exclusively by `OpaqueFrozenArtifactV1`, whose constructor is private.
