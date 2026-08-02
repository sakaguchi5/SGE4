# Unified Reconstruction Acceptance Matrix V1

| Gate | Requirement | Executable／Audit |
|---|---|---|
| U1 | complete Leaf bytes and Leaf Certificate correspond | `60_UnifiedArchitectureTests` |
| U2 | flat SGE4UNI 2.8 Composition Core and Composition Certificate correspond | `60_UnifiedArchitectureTests` |
| U3 | Debug A／Debug B／Release SGE4UNI 2.8 bytes match | `run_new_sge4_full_gate.bat` |
| U4 | ABI 2.8 Section／Core／Authority／embedded Leaf corruption is rejected | `60_UnifiedArchitectureTests` |
| U5 | exact initial／continue／recovery delta algebra | `60_UnifiedArchitectureTests` |
| U6 | all 40 carried invariants have a final owner | `62_UnifiedMigrationAcceptance` |
| U7 | Runtime Core has no Dynamic Planner／Verifier dependency | `tools/static_audit.py` |
| U8 | Leaf／Composition／Dynamic Planner has no corresponding Verifier dependency | `tools/static_audit.py` |
| U9 | Frozen Composition materializes on WARP | `61_UnifiedWindowsQualification` |
| U10 | pre-verified Frozen Invocation drives actual submission; different source-History identity is rejected | `61_UnifiedWindowsQualification` |
| G1-1 | Composition freezes one verified dense Leaf Dynamic Slot route | `60_UnifiedArchitectureTests`／`tools/static_audit.py` |
| G1-2 | exact Update payload set and payload identity are independently verified | `60_UnifiedArchitectureTests` |
| G1-3 | verified Update／Clear／Retain changes actual GPU-visible bytes | `61_UnifiedWindowsQualification` |
| G1-4 | caller collision, missing payload, and stale History are rejected before native submission | `61_UnifiedWindowsQualification` |
| G1-5 | Recovery clears runtime shadow and RecoverySeed fully rematerializes active members | `61_UnifiedWindowsQualification` |
| U11 | WARP readback before and after Recovery is equal | `61_UnifiedWindowsQualification` |
| U12 | epoch advances and old handles are rejected | `61_UnifiedWindowsQualification` |
| U13 | external rebind gate and RecoverySeed are mandatory | `61_UnifiedWindowsQualification` |
| U14 | actual Device removal and removed-adapter exclusion | `run_new_sge4_actual_removal_qualification.bat` |

現在の追加能力はVerified Dynamic Execution、Conditional Region、限定Texture2D Flow、Verified Indirect Work Execution、Multi-target Dynamic Routing、Verified Temporal Buffer Flowである。Texture一般化、Variant、Partial Recovery、Multiple AdapterはこのMatrixの対象外である。

## Level 4 Generalization 2

| ID | Acceptance | Gate |
|---|---|---|
| G2-1 | Conditional Region contract is canonical, non-nested and graph-closed | `60_UnifiedArchitectureTests` |
| G2-2 | Planner and independent Verifier derive identical selections and enabled Leaves | `60_UnifiedArchitectureTests` |
| G2-3 | Tampered enabled Leaf set is rejected | `60_UnifiedArchitectureTests` |
| G2-4 | True, zero-Leaf False, re-enable and RecoverySeed observations pass | `61_UnifiedWindowsQualification` |
| G2-5 | Debug A／Debug B／Release Frozen bytes match for SGE4UNI 2.8 | `run_new_sge4_full_gate.bat` |


## Level 4 Generalization 3

| ID | Acceptance | Gate |
|---|---|---|
| G3-1 | fixed BGRA8 Texture2D shape is canonical in Contract and Plan | `60_UnifiedArchitectureTests` |
| G3-2 | producer／consumer shape mismatch and ABI 1 Texture inference are rejected | `60_UnifiedArchitectureTests` |
| G3-3 | portable SGE4UNI 2.8 Texture Composition is deterministic and round-trips | `60_UnifiedArchitectureTests` |
| G3-4 | WARP executes RTV producer -> SRV consumer and packed readback matches | `61_UnifiedWindowsQualification` |
| G3-5 | controlled Recovery rematerializes the shared Texture and observation matches | `61_UnifiedWindowsQualification` |
| G3-6 | Debug A／Debug B／Release SGE4UNI 2.8 bytes match | `run_new_sge4_full_gate.bat` |


## Level 4 Generalization 4

| ID | Acceptance | Gate |
|---|---|---|
| G4-1 | SGE4UNI 2.8 Dynamic Contract schema 6 preserves one unconditional Compute Leaf／Command and maximum work count | `60_UnifiedArchitectureTests` |
| G4-2 | Planner and independent Verifier derive identical Dispatch arguments from exact Transition count | `60_UnifiedArchitectureTests` |
| G4-3 | Tampered X／workCount／identity is rejected before Freeze or submission | `60_UnifiedArchitectureTests` |
| G4-4 | WARP executes zero, three-work, retain-zero and RecoverySeed DispatchIndirect observations | `61_UnifiedWindowsQualification` |
| G4-5 | target ExecuteCompute is applied exactly once and other Commands retain fixed Dispatch | `61_UnifiedWindowsQualification` |
| G4-6 | Debug A／Debug B／Release Frozen bytes match for SGE4UNI 2.8／SGE4INV 1.6 | `run_new_sge4_full_gate.bat` |

## Level 4 Generalization 5

| ID | Acceptance | Gate |
|---|---|---|
| G5-1 | SGE4UNI 2.8 accepts fixed RGBA32F Texture2D UAV writer and freezes width／height／rowBytes | `60_UnifiedArchitectureTests` |
| G5-2 | Semantic／Reflection independently match StorageTexture2D and typed RWTexture2D | `60_UnifiedArchitectureTests` |
| G5-3 | Plan freezes UnorderedWrite → ShaderRead handoff and rejects format mismatch | `60_UnifiedArchitectureTests` |
| G5-4 | WARP observes RGBA32F UAV intermediate and BGRA8 SRV-consumer output | `61_UnifiedWindowsQualification` |
| G5-5 | Controlled Recovery rematerializes UAV Texture／descriptors and reproduces both readbacks | `61_UnifiedWindowsQualification` |
| G5-6 | Debug A／Debug B／Release Frozen bytes match for SGE4UNI 2.8／SGE4INV 1.6 | `run_new_sge4_full_gate.bat` |

## Level 4 Generalization 6

| ID | Acceptance | Gate |
|---|---|---|
| G6-1 | Dynamic Contract schema 6 freezes canonicalMemberBytes and canonical multi-route table | `60_UnifiedArchitectureTests` |
| G6-2 | SGE4INV 1.6 Execution Payload schema 2 binds all routes and Canonical Update payloads | `60_UnifiedArchitectureTests` |
| G6-3 | route order／duplicate target／out-of-range slice／payload width corruption is rejected | `60_UnifiedArchitectureTests` |
| G6-4 | Update／Clear is applied to all route shadows and committed atomically with History | `60_UnifiedArchitectureTests` |
| G6-5 | two GPU Leaves receive separate slices from one Canonical member payload | `61_UnifiedWindowsQualification` |
| G6-6 | controlled Recovery rebuilds all routed shadows and GPU outputs | `61_UnifiedWindowsQualification` |


## Level 4 Generalization 7

| ID | Acceptance | Gate |
|---|---|---|
| G7-1 | SGE4UNI 2.8 Contract／Decision schema 3 freezes TemporalHistory and history depth 1 | `60_UnifiedArchitectureTests` |
| G7-2 | Plan freezes two physical generations, Current writer and Previous readers | `60_UnifiedArchitectureTests` |
| G7-3 | Temporal Buffer is absent from same-frame handoff／signal／wait and invalid depth is rejected | `60_UnifiedArchitectureTests` |
| G7-4 | Portable SGE4UNI 2.8 Temporal Composition round-trips and ABI 1 migration rejects Temporal inference | `60_UnifiedArchitectureTests` |
| G7-5 | WARP observes seed→11, accepted Current→20, next-frame→21 | `61_UnifiedWindowsQualification` |
| G7-6 | Controlled Recovery invalidates temporal history and rebuilds Previous from explicit seed | `61_UnifiedWindowsQualification` |
| G7-7 | Debug A／Debug B／Release Frozen bytes match for SGE4UNI 2.8 | `run_new_sge4_full_gate.bat` |


## Level 5 Vertical Experiment 1

| ID | Acceptance | Gate |
|---|---|---|
| L5V1-1 | Dense／Sparse candidates contain identical Schema 17 Leaf set and Composition Contract | `63_Level5VerticalExperiment` |
| L5V1-2 | one 32-byte Canonical payload is routed to State Buffer and RGBA32F Texture paths | `63_Level5VerticalExperiment` |
| L5V1-3 | State Observation、accepted Temporal Aggregate、packed RGBA32F Texture digestが候補間で一致してからtiming sampleを受理する | `63_Level5VerticalExperiment --warp` |
| L5V1-4 | Previous Temporal Aggregate equals the prior successful frame and resets after Controlled Recovery | `63_Level5VerticalExperiment --warp` |
| L5V1-5 | State writer GPU timestamps are recorded in balanced A/B order for four K values | `run_sge4_level5_vertical_experiment.bat` |
| L5V1-6 | classification is evidence only and Owner decision remains DeferredByOwner | CSV evidence |


## Level 5 Vertical Experiment 2

| ID | Acceptance | Gate |
|---|---|---|
| L5V2-1 | Dense／Sparse candidates contain identical Schema 17 Leaf set and Composition Contract while only Sparse owns VerifiedU32 Compact Worklist authority | `64_Level5ArbitrarySparseWorklistExperiment` |
| L5V2-2 | Prefix／Suffix／UniformStride／Clustered4／fixed-seed Random are canonical sorted unique worksets for the same K | `64_Level5ArbitrarySparseWorklistExperiment --warp` |
| L5V2-3 | Frozen Compact Worklist member IDs equal the requested arbitrary Active／Modified set and drive DispatchIndirect count | `64_Level5ArbitrarySparseWorklistExperiment --warp` |
| L5V2-4 | State Observation、accepted Temporal Aggregate、packed RGBA32F Texture digest agree before timing samples are accepted | `64_Level5ArbitrarySparseWorklistExperiment --warp` |
| L5V2-5 | Workset digest／span／contiguous runs／gap metrics and balanced A-B／B-A raw timestamps are preserved in CSV | `run_sge4_level5_arbitrary_sparse_worklist_experiment.bat` |
| L5V2-6 | Controlled Recovery invalidates old worklist／Temporal state and rebuilds the fixed-seed arbitrary sparse set | `64_Level5ArbitrarySparseWorklistExperiment --warp` |
| L5V2-7 | classification is evidence only and Owner decision remains DeferredByOwner | CSV evidence |

## Level 4 Generalization 8

| ID | Acceptance | Gate |
|---|---|---|
| G8-1 | SGE4UNI 2.8 Dynamic Contract schema 6 freezes VerifiedU32 worklist mode and target index-list Dynamic Slot | `60_UnifiedArchitectureTests` |
| G8-2 | Dynamic Planner and independent Verifier derive identical canonical uint32 list from exact Transition set | `60_UnifiedArchitectureTests` |
| G8-3 | non-canonical order, identity tampering and dense-route／worklist Slot collision are rejected | `60_UnifiedArchitectureTests` |
| G8-4 | SGE4INV 1.6 Manifest schema 7 contains required execution-affecting Compact Worklist Section kind 8 | `60_UnifiedArchitectureTests` |
| G8-5 | Runtime materializes fixed-size list Slot with canonical indices followed by zero padding and rejects Caller collision | `60_UnifiedArchitectureTests`／`61_UnifiedWindowsQualification` |
| G8-6 | WARP executes arbitrary sparse InitialSeed, Update／Clear and RecoverySeed member IDs | `61_UnifiedWindowsQualification` |
| G8-7 | Debug A／Debug B／Release Frozen bytes match for SGE4UNI 2.8／SGE4INV 1.6 | `run_new_sge4_full_gate.bat` |
