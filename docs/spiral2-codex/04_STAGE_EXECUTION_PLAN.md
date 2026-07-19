# Stage execution plan

## Rule

Proceed in order. Do not mark a Stage complete merely because code builds. Each Stage requires its intended positive path, negative gate, evidence, and relevant regression.

| Stage | CU | Name | Required work | Primary deliverable | Completion gate |
|---|---|---|---|---|---|
| S2-00 | CU0 | Decision Closure | Record the Owner's post-baseline selection of FixedDynamicMotorPaletteAndHierarchy without rewriting Spiral 1 history. | Selection record and milestone identity | Research-contract verifier |
| S2-01 | CU0 | Completion Specification | Freeze scope, non-goals, schemas, candidate graphs, observation, corpus, authority, negative gates, recovery, invariants, and Stage map. | Canonical Spiral 2 completion specification | Document/contract hash gate |
| S2-02 | CU0 | Foundation Baseline Freeze | Freeze baseline commit, Level 4 v1 and Spiral 1 evidence, Schema/Runtime versions, dependency graph, and runners. | Baseline evidence ledger | Foundation, Gate, strongest available Freeze |
| S2-03 | CU1 | Canonical Hierarchy Semantic | Implement hierarchy builder, validation, canonical order, depth derivation, serialization, identity, and mutation tests. | 80_HierarchySemantic | Semantic determinism and negative gate |
| S2-04 | CU1 | Dynamic Motor Palette Authority | Implement dynamic palette builder and exact invocation schema; prove value changes do not change Frozen artifacts. | Dynamic palette and multi-submit tests | Dynamic authority gate |
| S2-05 | CU1 | Corpus and CPU Reference | Implement H00-H08 and F00-F11 with fixed bytes and independent binary64 reference. | 82_Spiral2Corpus | Fresh-process corpus determinism |
| S2-06 | CU2 | Three Candidate Graphs | Generate Matrix, Direct PGA, and Hybrid graphs from one Semantic identity. | 83/84 candidate and planner | Candidate identity and structure tests |
| S2-07 | CU2 | Independent Hierarchy Verifier | Independently rederive graph and lowering facts; seal only verified graphs. | 85_HierarchyRepresentationVerifier | Raw graph non-executable gate |
| S2-08 | CU2 | Authority Mutation Suite | Reject topology, layer, kind, order, stride, program, slot, count, contract, seal, semantic, and profile mutations. | Mutation ledger and tests | All mutations rejected at intended boundary |
| S2-09 | CU3 | Dynamic Source Vertical Slice | Use 2 bones, 2 frames, 1 branch to test existing Schema 17/Runtime 17 and Level 4 dynamic path. | Sufficiency finding | No hidden Runtime workaround |
| S2-10 | CU3 | Matrix Leaf Group | Qualify A0-A2 from identical Motor bytes; no CPU Matrix Palette. | Matrix Frozen Leaf group | Leaf-only WARP and rematerialization |
| S2-11 | CU3 | Direct PGA Leaf Group | Qualify B0-B1 with no Matrix constants or intermediates. | Direct PGA Frozen Leaf group | Leaf-only WARP and reference contract |
| S2-12 | CU3 | Hybrid Leaf Group | Qualify C0-C2 with actual global Motor buffer and verified conversion boundary. | Hybrid Frozen Leaf group | Leaf-only WARP and boundary authority |
| S2-13 | CU4 | Ten-Leaf Frozen Comparison | Connect all branches and comparison into a static single-writer one-domain Composition. | Frozen comparison Composition | Composition authority tests |
| S2-14 | CU4 | Level 4 Sufficiency Gate | Formally decide existing sufficiency or implement only the evidence-required minimal canonical extension. | Sufficiency/extension decision record | Level 4 + Spiral 1 full regression |
| S2-15 | CU5 | WARP Hierarchy Corpus | Run all topology/frame combinations and all pairwise/reference/rigidity checks. | WARP evidence corpus | Zero non-finite and zero contract mismatch |
| S2-16 | CU5 | Multi-frame Dynamic Invocation | Submit successive palettes to one Frozen Composition; prove no compile and no stale frame contamination. | Multi-frame evidence | Order and repeatability tests |
| S2-17 | CU5 | Determinism Freeze | Compare Debug fresh process A/B and Release for all Frozen artifacts, excluding dynamic values. | Determinism manifests | Byte-identical gate |
| S2-18 | CU5 | Recovery Qualification | Controlled and actual removal, excluded LUID, fresh-process load, dynamic rebind, stale epoch rejection. | Recovery evidence | Controlled and actual recovery gate |
| S2-19 | CU6 | Architecture Final Freeze | Close S2-I01-S2-I22 and emit Architecture banner. | Architecture final freeze | Spiral 2 Freeze runner |
| S2-20 | CU6 | Real GPU Measurement | Measure M0-M5 with balanced six-order execution and full environment fingerprint. | Binary evidence and report | Real adapter measurement |
| S2-21 | CU6 | Decision Evidence Report | Answer all twelve roadmap questions from evidence; rank and optionally recommend next candidates. | Decision Evidence Report | Report schema and evidence digests |
| S2-22A | CU6 | Evidence Closure | Close all work except authoritative next-capability selection; preserve DeferredByOwner. | Measurement and Decision Evidence Complete | Deferred selection verifier |
| S2-22B | Owner | Next Capability Selection | Owner selects the next Spiral or Level 4 extension after reviewing the evidence. | Owner selection record and final Experiment Complete banner | OWNER_DECISION_REQUIRED |

## Completion Unit cadence

At the end of each CU:

1. run narrow project tests during repair,
2. run the Spiral 2 CU runner,
3. run existing `run_sge4_5_gate.bat`,
4. run Regression when Package, Runtime, Backend, Composition, or recovery changed,
5. perform the self-audit,
6. update evidence and progress ledgers,
7. update and verify `SOURCE_MANIFEST.sha256`,
8. create one intentional CU commit when commits are allowed.

## Critical path

```text
CU0 contract and baseline
-> CU1 semantic, invocation, corpus
-> CU2 candidate and verifier authority
-> CU3 vertical slice and three Leaf groups
-> CU4 comparison Composition and Level 4 sufficiency
-> CU5 WARP, multi-frame, determinism, recovery
-> CU6 Architecture, measurement, decision evidence
-> Owner-only next-capability selection
```

Do not build all three candidates before the S2-09 vertical slice has proven the dynamic boundary or documented its exact insufficiency.
