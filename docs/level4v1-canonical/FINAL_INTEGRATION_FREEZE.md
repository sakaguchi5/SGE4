# SGE4 Level 4 v1 Final Integration Freeze

## Canonical build root

`SemanticGpuEngine4.sln` is the sole canonical solution for Level 4 v1.
Every repository `.vcxproj`, including the R1-R5 Composition projects and their
qualification executables, is registered exactly once in this solution with
Debug x64 and Release x64 build mappings.

The stage-specific solutions were temporary construction views and are not
part of the completed repository. Their history remains available in version
control, while every canonical qualification runner uses only the main solution.

## Final integration boundary

The integration changes no Frozen Composition ABI, Contract semantics,
Verified Plan, DeviceDomain runtime behavior, Recovery behavior, or R1-R5 test
scenario. It closes only the repository-level authority boundary:

```text
SemanticGpuEngine4.sln
  -> Debug and Release full build
  -> R1 Frozen Artifact boundary
  -> R2 Contract and independent verification
  -> R3 Shared DeviceDomain runtime
  -> R4 Whole-composition recovery
  -> R5 canonical qualification
  -> Architecture and SOURCE_MANIFEST
  -> Stage 0D Foundation Freeze
```

## Completion command

```powershell
.\run_sge4_level4v1_final.bat
```

A zero exit status and the following final banner close Level 4 v1:

```text
SGE4 LEVEL 4 V1 CANONICAL FINAL INTEGRATION FREEZE PASSED
```

## Numbering

The Composition projects retain their already-qualified directory names
`20_CompositionDeviceDomain` through `23_CompositionRecovery`. The main
solution also contains `20_ExperimentDomain` through `23_PgaFrontend`.
Names, paths, and project GUIDs are all unique, so this is not a Visual Studio
identity collision. Renaming qualified projects solely to remove visual number
overlap would create unnecessary source and project-reference churn.

## Stability watch

Two intermittent WARP `DXGI_ERROR_DEVICE_REMOVED` observations occurred during
long aggregate Foundation Freeze runs, once near Slice 14 and once near Slice
15. The same binaries subsequently passed standalone Freeze, no-build R5, and
complete aggregate execution. This is recorded as a stability watch rather
than a completion blocker. The failing logs must be retained and recurrence
frequency should be reviewed before any retry policy is introduced.
