# F0 correction record

## Correction 1

The first Foundation Bootstrap archive retained `using namespace sge4;` in
`46_CanonicalCompositionArtifactTests/main.cpp`. This prevented the renamed
SGE4-5 solution from compiling.

The reference is now `using namespace sge4_5;`. The independent identity
checker also rejects old namespace declarations, using-directives, and
qualified `sge4::` references.

The correction changes no package, plan, composition, runtime, or recovery
semantics. It only completes the independent namespace rename.
