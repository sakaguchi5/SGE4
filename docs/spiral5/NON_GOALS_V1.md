# Spiral 5 explicit non-goals v1

Spiral 5 V1 studies exact piecewise-constant temporal state and one-generation history reuse.

It does not study:

- interpolation or extrapolation,
- motion prediction or approximate temporal quality,
- multiple retained generations,
- arbitrary access to old frames,
- ring Buffers or frame-history arrays,
- omitted Local Motor payloads on hold invocations,
- variable Work count, sparse Work, culling, or compaction,
- representation-path comparison,
- variable hierarchy topology,
- Texture, Raster, image, or presentation history,
- game-derived update schedules,
- distance, visibility, importance, or animation LOD,
- general Runtime Conditional Regions,
- Runtime adaptive Candidate selection,
- multiple DeviceDomains, partial recovery, streaming, or residency,
- a universal claim that caching or hold execution is faster.

Interpolation is explicitly excluded because it is a different mathematical temporal meaning. It may be studied later only after its Semantic and Observation Contract are independently frozen.
