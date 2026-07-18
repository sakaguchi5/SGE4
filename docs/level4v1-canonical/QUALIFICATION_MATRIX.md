# SGE4 Level 4 v1 Canonical Qualification Matrix

| Invariant group | Positive evidence | Negative evidence | Runtime evidence |
|---|---|---|---|
| ABI | ReadOnly and WriteOnly Buffer endpoints derived from validated Leaves | forged access, missing declaration, duplicate declaration, mismatched slot/resource/state/size | embedded executable Leaf remains independently decodable |
| Contract | input, output, linear, fan-out, and diamond contracts | missing endpoint, double binding, multiple writer, incompatible shape, duplicate stable key, two presenters | none; contract tests are target-object free |
| Planning | canonical allocation, schedule, binding, handoff, signal, and wait proposal | cycle rejection | none; proposal tests are target-object free |
| Verification | independently sealed valid plan | mutation of every represented decision, including recomputed raw digest | none; verifier tests are target-object free |
| Artifact | deterministic write/read/write identity | header, size, section, digest, identity, certificate, Leaf, and decision corruption | fresh-process load without Compiler or Planner |
| DeviceDomain | all Leaves loaded into one active epoch | missing presenter surface, forged artifact, incompatible profile | D3D12 WARP DeviceDomain materialization |
| Shared Buffer | one native Buffer per Resource Flow | duplicate or oversized initial data, wrong domain, stale epoch, wrong Resource identity | upload/readback and explicit transition on WARP |
| Static runtime | independent, linear, fan-out, multi-input, diamond, headless, presenter, repeated frame | duplicate dynamic binding, missing binding, wrong Leaf/slot, frozen mutation | expected readback values and token retirement |
| Recovery | controlled whole-composition rematerialization | retry while Active, stale Resource/token, inactive Submit/Readback | actual RemoveDevice, removed-LUID exclusion, AwaitingAdapter, fresh process |
| Determinism | Debug A == Debug B == Release | any byte mismatch fails | evidence manifests and frozen artifacts |
| Regression | complete existing Level 1-3 suite | architecture and source inventory failures stop qualification | existing WARP and recovery observations remain accepted |

## Required WARP observations

- Independent Leaves do not create false dependencies.
- A producer-to-consumer Buffer handoff returns the expected data.
- Fan-out readers observe the same producer result.
- A multi-input consumer waits for both predecessors.
- The diamond graph produces its fixed expected readback vector.
- A headless producer may precede the sole presenter.
- Repeated frames do not allow a writer to overtake an unfinished fan-out reader.
- Controlled rebuild advances the epoch and produces the same post-recovery result.
- Actual RemoveDevice enters AwaitingAdapter when only the removed WARP remains.
- Submit and Readback fail while AwaitingAdapter.
- A fresh process loads the frozen composition and reproduces the qualified result.

## Completion accounting

Every ledger ID must be referenced by at least one test name in the final test
inventory. The final runner must emit a machine-readable manifest containing:

- source tree digest;
- frozen composition digest;
- proof-ledger revision;
- Debug and Release configuration identity;
- passed positive scenarios;
- passed negative mutations;
- recovery observations;
- preserved Level 1-3 corpus identity.
