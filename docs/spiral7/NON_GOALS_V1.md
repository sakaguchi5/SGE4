# Spiral 7 V1 non-goals

Spiral 7 V1 does not infer or implement game policy. It does not decide visibility, culling, distance, priority, LOD, importance, animation intent, or which records changed.

The exact current Active Set and exact modified-survivor set are external verified inputs.

Spiral 7 V1 also excludes:

- interpolation or prediction between generations,
- approximate, probabilistic or lossy omission,
- GPU discovery of membership or change,
- GPU prefix-sum compaction as the source of semantic truth,
- dynamic Resource capacity,
- compacted final output order,
- multiple history depths,
- multiple Writers, reduction or atomic merge semantics,
- conditional Composition regions or Variant selection,
- Texture/Raster/meshlet/BVH specialization,
- streaming and residency,
- Partial Recovery,
- multiple DeviceDomains or adapters,
- Runtime adaptive Candidate selection,
- a universal claim that incremental history reuse is faster.

The Runtime and Backend may execute a verified Candidate. They may not choose Active membership, modified membership, transition action, Candidate kind, update granularity or history policy.
