# Spiral 5 CU3 — independent Temporal authority

CU2 proved one physical `GlobalMotorHistoryReuse` path. CU3 removes trust from the canonical construction route.

## Authority chain

```text
TemporalStateSemanticV1 + UpdateScheduleV1
  -> RawTemporalLoweringCandidateV1
  -> TemporalLoweringPlanner

TemporalStateSemanticV1 + UpdateScheduleV1 + Verification Context
  -> Planner-independent TemporalLoweringVerifier
  -> opaque VerifiedTemporalLoweringV1
  -> CU2 Frozen Temporal Artifact
  -> actual Global Motor History Resource binding
  -> device-epoch-bound Frozen verified execution
  -> WARP execution
```

The Verifier has no project or source dependency on the Planner. It independently reconstructs the CU2 Temporal artifact, the 128 invocation generation/dispatch authority table, the expected Program identities, and the expected Global Motor History Resource identity.

## Opaque type boundary

`VerifiedTemporalLoweringV1` and `FrozenVerifiedGlobalMotorHistoryExecutionV1` have private constructors. A Raw Candidate cannot be implicitly converted into either type and cannot execute or Freeze.

## Generation authority

The schedule identity alone is not treated as sufficient evidence. CU3 also freezes the SHA-256 identity of the independently derived invocation table containing:

- invocation index,
- Update/Hold event,
- source generation,
- hierarchy Dispatch enablement,
- Consumer Dispatch,
- history read generation,
- history write generation,
- retained completion ordinal.

A Candidate with a correct schedule identity but a different invocation authority identity is rejected, including after an attacker recomputes the Raw Candidate identity.

## Resource and epoch binding

The Verified Plan states the required History Resource contract. Freeze then binds it to the actual CU2 artifact-derived Global Motor History Resource identity and one nonzero device epoch.

The Frozen identity covers:

- Verified Plan and Verifier certificate,
- CU2 Temporal artifact identity,
- actual History Resource identity,
- History Role, depth, and byte size,
- device epoch.

A Frozen binding for epoch 1 is invalid when replayed as epoch 2. CU3 proves the identity boundary; actual Recovery and reseed behavior remain CU5 work.

## Runtime boundary

Runtime and Backend still do not decide Update versus Hold, history placement, source generation, or Dispatch enablement. Those values are frozen by the verified schedule and invocation authority.
