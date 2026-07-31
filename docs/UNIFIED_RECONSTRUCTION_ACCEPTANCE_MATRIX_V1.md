# Unified Reconstruction Acceptance Matrix V1

| Gate | Requirement | Executable／Audit |
|---|---|---|
| U1 | complete Leaf bytes and Leaf Certificate correspond | `60_UnifiedArchitectureTests` |
| U2 | flat SGE4UNI 2.0 Composition Core and Composition Certificate correspond | `60_UnifiedArchitectureTests` |
| U3 | Debug A／Debug B／Release SGE4UNI 2.0 bytes match | `run_new_sge4_full_gate.bat` |
| U4 | ABI 2.0 Section／Core／Authority／embedded Leaf corruption is rejected | `60_UnifiedArchitectureTests` |
| U5 | exact initial／continue／recovery delta algebra | `60_UnifiedArchitectureTests` |
| U6 | all 40 carried invariants have a final owner | `62_UnifiedMigrationAcceptance` |
| U7 | Runtime Core has no Dynamic Planner／Verifier dependency | `tools/static_audit.py` |
| U8 | Leaf／Composition／Dynamic Planner has no corresponding Verifier dependency | `tools/static_audit.py` |
| U9 | Frozen Composition materializes on WARP | `61_UnifiedWindowsQualification` |
| U10 | pre-verified Frozen Invocation drives actual submission; different source-History identity is rejected | `61_UnifiedWindowsQualification` |
| U11 | WARP readback before and after Recovery is equal | `61_UnifiedWindowsQualification` |
| U12 | epoch advances and old handles are rejected | `61_UnifiedWindowsQualification` |
| U13 | external rebind gate and RecoverySeed are mandatory | `61_UnifiedWindowsQualification` |
| U14 | actual Device removal and removed-adapter exclusion | `run_new_sge4_actual_removal_qualification.bat` |

New capabilityはNone。Texture Flow、Variant、Partial Recovery、Multiple AdapterはこのMatrixの対象外である。
