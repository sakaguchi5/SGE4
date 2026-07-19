# Spiral 2 CU2 evidence ledger

Stages: S2-06, S2-07, S2-08

- Candidate A: source -> local Motor-to-Matrix -> Matrix hierarchy -> Matrix apply
- Candidate B: source -> Motor hierarchy -> direct PGA apply
- Candidate C: source -> Motor hierarchy -> global Motor-to-Matrix -> Matrix apply
- All candidates bind one identical 32-byte-per-bone canonical Motor slot.
- Raw graphs expose no Freeze, Execute, or Submit entry point.
- `85_HierarchyRepresentationVerifier` has no reference to Planner and independently
  derives node kinds/order, strides, programs, edges, lowering position, hierarchy
  depth, dispatch count, semantic identity, observation identity, and slot identity.
- 27 H00-H08 candidate structures sealed successfully.
- 15 authority mutations were rejected at the Verifier boundary.

```text
SelfAudit = Passed
IndependentSoftwareVerifier = Implemented
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
