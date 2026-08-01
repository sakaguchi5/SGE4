# Unified Reconstruction Acceptance Matrix V1

| Gate | Requirement | Executable／Audit |
|---|---|---|
| U1 | complete Leaf bytes and Leaf Certificate correspond | `60_UnifiedArchitectureTests` |
| U2 | flat SGE4UNI 2.2 Composition Core and Composition Certificate correspond | `60_UnifiedArchitectureTests` |
| U3 | Debug A／Debug B／Release SGE4UNI 2.2 bytes match | `run_new_sge4_full_gate.bat` |
| U4 | ABI 2.2 Section／Core／Authority／embedded Leaf corruption is rejected | `60_UnifiedArchitectureTests` |
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

New capabilityはVerified Dynamic Execution。Texture Flow、Variant、Partial Recovery、Multiple AdapterはこのMatrixの対象外である。

## Level 4 Generalization 2

| ID | Acceptance | Gate |
|---|---|---|
| G2-1 | Conditional Region contract is canonical, non-nested and graph-closed | `60_UnifiedArchitectureTests` |
| G2-2 | Planner and independent Verifier derive identical selections and enabled Leaves | `60_UnifiedArchitectureTests` |
| G2-3 | Tampered enabled Leaf set is rejected | `60_UnifiedArchitectureTests` |
| G2-4 | True, zero-Leaf False, re-enable and RecoverySeed observations pass | `61_UnifiedWindowsQualification` |
| G2-5 | Debug A／Debug B／Release Frozen bytes match for SGE4UNI 2.2 | `run_new_sge4_full_gate.bat` |
