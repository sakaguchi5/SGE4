# Spiral 7 CU3 mutation and replay matrix

## Raw proposal attacks — 50

The test mutates and rehashes every material identity, policy and numeric field, including:

- Semantic, Invocation, Previous Active, Active, Modified, Transition Set and Transition Record identities,
- Target, Observation, Delta/History resource contracts, Completion and Device epoch policy,
- Spiral 4/5/6 identities, Program, Artifact and three proposed resource identities,
- Transition authority,
- Candidate kind, representation role, operation code, History policy, completion and epoch policies,
- Invocation, universe, Update/Clear/Retain/Transition counts, Dispatch and capacity fields,
- the un-rehashed Raw identity itself.

All 50 attacks must be rejected by the Planner-independent Verifier.

## Verified replay gates — 4

- cross-Invocation replay,
- cross-Target replay,
- cross-Observation replay,
- cross-Previous-History-resource-contract replay.

## Frozen resource, artifact and epoch gates — 14

- wrong Delta, previous History or output History identity,
- wrong Delta/History roles,
- wrong capacity, stride or History byte sizes,
- zero Device epoch,
- aliased previous/output History identity,
- cross-Invocation Artifact,
- cross-Device-epoch replay.
