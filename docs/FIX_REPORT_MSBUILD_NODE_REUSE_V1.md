# Fix Report — MSBuild Node Reuse

## Observed behavior

`run_new_sge4_full_gate.bat` completed successfully, but one or more `MSBuild.exe` processes remained after all qualification executables had exited.

## Cause

The build scripts used `/m` without disabling node reuse. MSBuild enables node reuse by default, so parallel worker nodes may remain available for a later command-line build. This is expected MSBuild behavior, but it is undesirable for a self-contained qualification gate.

## Correction

The following scripts now pass `/nr:false` to every direct MSBuild invocation:

- `build_new_sge4.bat`
- `run_new_sge4_architecture_tests.bat`
- `run_new_sge4_windows_qualification.bat`
- `run_new_sge4_actual_removal_qualification.bat`

The full gate calls `build_new_sge4.bat`, so it inherits the correction. Parallel build `/m` remains enabled.

## Deliberately not used

No `taskkill` or global process cleanup command was added. Such a command could terminate an unrelated build or a Visual Studio-owned MSBuild process. The fix controls only the lifetime of nodes created by these command-line invocations.

## Product impact

None. No C++ source, package bytes, authority identity, verifier behavior, runtime state machine, D3D12 execution path, or qualification expectation changed.
