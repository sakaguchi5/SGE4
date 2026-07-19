# SGE4 Qualification Matrix

| Proven property | Primary executable |
|---|---|
| Semantic graph validity, builder rules, dependency/effect analysis | `30_SemanticContractTests` |
| Container format, section/digest validation, corruption rejection | `31_PackageContractTests` |
| Source validation, shader compile/reflection, generic Package production | `32_CompilerPipelineTests` |
| Candidate generation, policy, profile, Pareto frontier, SGE3 canonical and planning compatibility | `33_PlanningTests` |
| Verifier seal and Plan-field authority | `34_AuthorityTests` |
| Every required Operation is understood; unknown Operation rejected | `35_D3D12ConformanceTests` |
| Buffer/Texture GPU upload, readback, fresh-process rematerialization, recovery | `36_D3D12ReadbackTests` |
| Dynamic slots, external resources, temporal semantics, invocation validation | `37_RuntimeSemanticTests` |
| Invalid cross-layer and Package boundary cases are rejected | `38_AdversarialBoundaryTests` |
| Classical/SDF/PGA mathematical convergence | `39_FrontendEquivalenceTests` |
| Slice 1-15 source/Package scenario contracts | `40_SliceScenarioTests` |
| WARP execution of the proven vertical slices | `41_SliceExecutionTests` |
| Source-ID/order transformations preserve canonical output | `42_MetamorphicTests` |
| Generated finite Graph corpus and determinism | `43_GeneratedGraphTests` |
| Fixed 54-Package canonical freeze manifest | `44_CanonicalFreezeTests` |
| Accepted alternative schedule/queue Plans preserve WARP observations | `45_PlannedRuntimeTests` |

`run_sge4_5_stage0d.bat` is the authoritative Stage-0D freeze command. It also invokes `verify_dependencies.ps1` and compares fresh-process Debug/Release manifests byte-for-byte.
