# SGE4-5 Foundation Requalification Checklist

- [ ] `SemanticGpuEngine4-5.sln` builds in Debug x64.
- [ ] `SemanticGpuEngine4-5.sln` builds in Release x64.
- [ ] No `namespace sge4` production declaration remains.
- [ ] No old `12_SGE4Compiler` project remains.
- [ ] All project GUIDs are unique and differ from the SGE4v1 source archive.
- [ ] `verify_dependencies.ps1` passes.
- [ ] `tests/tools/Verify-ScriptContracts.ps1` passes.
- [ ] `tests/tools/Verify-SourceManifest.ps1` passes.
- [ ] Level 1-3 planning and authority regression passes.
- [ ] Level 4 v1 composition artifact and authority tests pass.
- [ ] Composition runtime and recovery tests pass.
- [ ] Fresh Debug A / Debug B / Release frozen outputs are byte-identical within SGE4-5.
