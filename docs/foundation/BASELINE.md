# SGE4-5 Foundation Baseline

- Product name: **SGE4-5**
- Source archive: `SGE4v1.zip`
- Source archive SHA-256: `52e0567bbd86cef2a1840e09d3a9f0d5efc128b4170ceb495a4905ab6d659894`
- Source archive root: `SGE4v1/`
- Foundation stage: `F0 - Foundation Bootstrap`
- Independent solution: `SemanticGpuEngine4-5.sln`
- Independent C++ namespace: `sge4_5`

## Authority of the baseline

This tree was created directly from `SGE4v1.zip`. It does not use `SGE4.zip` or a reconstructed Git checkout as its source.

The original source inventory is preserved in:

- `docs/foundation/SGE4v1_SOURCE_MANIFEST.sha256`

The active SGE4-5 inventory is:

- `SOURCE_MANIFEST.sha256`

## Inherited frozen identities

Level 4 v1 package/composition format literals and domain separators that encode the proven SGE4 Level 4 v1 ABI are intentionally retained unless an identity is only a build, namespace, solution, script, or product-shell identifier. This lets SGE4-5 inherit the proven Level 4 v1 execution boundary without silently inventing a new ABI during Foundation Bootstrap.
