# Unified Reconstruction Acceptance Matrix V1

| Gate | Requirement | Executable／Audit |
|---|---|---|
| U1 | complete Leaf bytes and Leaf Certificate correspond | `60_UnifiedArchitectureTests` |
| U2 | flat SGE4UNI 2.5 Composition Core and Composition Certificate correspond | `60_UnifiedArchitectureTests` |
| U3 | Debug A／Debug B／Release SGE4UNI 2.5 bytes match | `run_new_sge4_full_gate.bat` |
| U4 | ABI 2.5 Section／Core／Authority／embedded Leaf corruption is rejected | `60_UnifiedArchitectureTests` |
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

現在の追加能力はVerified Dynamic Execution、Conditional Region、限定Texture2D Flow、Verified Indirect Work Executionである。Texture一般化、Variant、Partial Recovery、Multiple AdapterはこのMatrixの対象外である。

## Level 4 Generalization 2

| ID | Acceptance | Gate |
|---|---|---|
| G2-1 | Conditional Region contract is canonical, non-nested and graph-closed | `60_UnifiedArchitectureTests` |
| G2-2 | Planner and independent Verifier derive identical selections and enabled Leaves | `60_UnifiedArchitectureTests` |
| G2-3 | Tampered enabled Leaf set is rejected | `60_UnifiedArchitectureTests` |
| G2-4 | True, zero-Leaf False, re-enable and RecoverySeed observations pass | `61_UnifiedWindowsQualification` |
| G2-5 | Debug A／Debug B／Release Frozen bytes match for SGE4UNI 2.5 | `run_new_sge4_full_gate.bat` |


## Level 4 Generalization 3

| ID | Acceptance | Gate |
|---|---|---|
| G3-1 | fixed BGRA8 Texture2D shape is canonical in Contract and Plan | `60_UnifiedArchitectureTests` |
| G3-2 | producer／consumer shape mismatch and ABI 1 Texture inference are rejected | `60_UnifiedArchitectureTests` |
| G3-3 | portable SGE4UNI 2.5 Texture Composition is deterministic and round-trips | `60_UnifiedArchitectureTests` |
| G3-4 | WARP executes RTV producer -> SRV consumer and packed readback matches | `61_UnifiedWindowsQualification` |
| G3-5 | controlled Recovery rematerializes the shared Texture and observation matches | `61_UnifiedWindowsQualification` |
| G3-6 | Debug A／Debug B／Release SGE4UNI 2.5 bytes match | `run_new_sge4_full_gate.bat` |


## Level 4 Generalization 4

| ID | Acceptance | Gate |
|---|---|---|
| G4-1 | SGE4UNI 2.5 schema 4 fixes one unconditional Compute Leaf／Command and maximum work count | `60_UnifiedArchitectureTests` |
| G4-2 | Planner and independent Verifier derive identical Dispatch arguments from exact Transition count | `60_UnifiedArchitectureTests` |
| G4-3 | Tampered X／workCount／identity is rejected before Freeze or submission | `60_UnifiedArchitectureTests` |
| G4-4 | WARP executes zero, three-work, retain-zero and RecoverySeed DispatchIndirect observations | `61_UnifiedWindowsQualification` |
| G4-5 | target ExecuteCompute is applied exactly once and other Commands retain fixed Dispatch | `61_UnifiedWindowsQualification` |
| G4-6 | Debug A／Debug B／Release Frozen bytes match for SGE4UNI 2.5／SGE4INV 1.4 | `run_new_sge4_full_gate.bat` |

| G5-1 | SGE4UNI 2.5 accepts fixed RGBA32F Texture2D UAV writer and freezes width／height／rowBytes | `60_UnifiedArchitectureTests` |
| G5-2 | Semantic／Reflection independently match StorageTexture2D and typed RWTexture2D | `60_UnifiedArchitectureTests` |
| G5-3 | Plan freezes UnorderedWrite → ShaderRead handoff and rejects format mismatch | `60_UnifiedArchitectureTests` |
| G5-4 | WARP observes RGBA32F UAV intermediate and BGRA8 SRV-consumer output | `61_UnifiedWindowsQualification` |
| G5-5 | Controlled Recovery rematerializes UAV Texture／descriptors and reproduces both readbacks | `61_UnifiedWindowsQualification` |
| G5-6 | Debug A／Debug B／Release Frozen bytes match for SGE4UNI 2.5／SGE4INV 1.4 | `run_new_sge4_full_gate.bat` |
