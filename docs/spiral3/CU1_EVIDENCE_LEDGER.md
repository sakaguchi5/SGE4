# Spiral 3 CU1 evidence ledger

Scope: fixed hierarchy, typed reuse workload, deterministic point corpus and independent CPU reference.

Implemented evidence:

- explicit and non-convertible `BoneCountV1`, `ReuseCountV1`, and `PointCountV1` boundary types;
- checked `FixedReuseWorkloadShapeV1`, including overflow and 4096-point qualification bound;
- R1/R4/R16/R64/R256/R512 on one eight-bone canonical hierarchy;
- deterministic bone-major point corpus and independent binary64 dual-quaternion reference;
- zero/over-bound workload rejection and six distinct Semantic identities;
- portable C++23 compile/run qualification in the generation environment.

Windows completion command: `run_sge4_5_spiral3_cu1_reuse_semantic.bat`.
