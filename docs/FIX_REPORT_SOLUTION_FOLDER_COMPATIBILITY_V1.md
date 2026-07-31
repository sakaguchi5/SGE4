# Solution Folder Compatibility Fix v1

## Symptom

The Visual Studio/MSBuild project summary reported `Product` and `Tests` as failed projects while all real C++ projects succeeded. The reported project type GUID was `{66A26720-8FB5-11D2-AA7E-00C04F688DDE}`.

## Cause

`Product` and `Tests` were virtual Solution Folder entries in `NewSGE4.sln`, not buildable projects. The user's project-reporting environment attempted to resolve those virtual entries as ordinary project types and reported them as unsupported.

## Fix

The two virtual Solution Folder entries and the associated `NestedProjects` section were removed from `NewSGE4.sln`.

No `.vcxproj`, C++ source, project dependency, ABI, test, runtime, or D3D12 behavior was changed. The solution now contains only the 17 real MSBuild projects.
