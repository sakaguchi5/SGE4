# Spiral 6 CU3 mutation matrix

The authority test rejects 36 Raw proposal attacks. Except for the direct Raw-identity corruption case, the attacker recomputes the Raw identity after mutation.

Attacked dimensions include:

- Semantic, exact set, Target, Observation, Resource-contract, Completion and device-epoch-policy identities;
- Spiral 4 indirect artifact, Consumer Program, Sparse sidecar artifact, actual index Resource and write-set authority identities;
- Candidate kind, representation role, operation code, output initialization, write-set policy, completion policy and epoch policy;
- universe size, cardinality, thread-group size, Dispatch groups, index stride, fixed Resource capacity, output capacity and stride;
- unused-tail sentinel and `K=0 -> Dispatch 0` rule;
- adversarial boundary values for zero/full cardinality, zero Dispatch and altered capacities.

Separate replay gates reject cross-set, cross-Target, cross-Observation, cross-artifact, cross-Resource, cross-role, cross-layout and cross-device-epoch use.
