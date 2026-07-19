# SGE4-5 Spiral 2 completion specification v0.1

Status: Frozen Research Contract

## Objective

Prove that one fixed rigid hierarchy and per-frame canonical binary32 Local PGA
Motor bytes can generate three independently verified Leaf groups, enter one
Level 4 v1 Frozen Composition, execute across frames, recover, remain
deterministic, and produce fair WARP and hardware evidence.

## Identity boundary

Frozen identity includes bone count, parent table, root, canonical order, depth
layers, Motor convention, dynamic schema, probe/output schemas, Observation
Contract, candidate graph kind, program template versions, and target profile.
It excludes Motor values, frame number, dynamic reference output, and invocation
identity. Bone count or topology changes require recompilation; palette-value
changes do not.

## Canonical hierarchy

`RigidHierarchySemanticV1` is one finite acyclic tree with one root,
`parentIndex < childIndex`, fixed topology, canonical bone order, and canonical
depth layers. It contains no Matrix, Shader, Queue, descriptor, allocation,
resource state, or dynamic Motor values.

## Dynamic input

`DynamicLocalMotorPaletteV1` contains exactly `boneCount` records of eight
binary32 coefficients in `qr.w,x,y,z; qd.w,x,y,z` order. The Builder rejects
non-finite values, zero or non-unit rotations, noncanonical sign, size/count
mismatch, and unknown convention. Every candidate reads the same bytes.

## Candidate graphs

- Matrix: Local Motor to Matrix, Matrix Hierarchy, Matrix Probe Apply; 48-byte intermediate.
- Direct PGA: Motor Hierarchy, Direct PGA Probe Apply; 32-byte intermediate and no Matrix materialization.
- Hybrid: Motor Hierarchy, Global Motor to Matrix, Matrix Probe Apply; Matrix materialization only after hierarchy.

Raw graphs are proposals only. The independent Hierarchy Verifier rederives the
topology, depth layers, roles, flows, materialization position, schemas, strides,
program and binding digests, observation, and target profile. Only its opaque,
context-bound seal can enter the Leaf Compiler.

## Observation

Every global transform is applied to P0 `(0,0,0,1)`, P1 `(1,0,0,1)`, P2
`(0,1,0,1)`, P3 `(0,0,1,1)`, and P4 `(0.37,-0.61,1.13,1)`. An independent
binary64 CPU path produces the reference. Evidence records reference and
pairwise absolute/relative/ULP errors, non-finite values, axis length,
orthogonality, orientation, and translation.

## Composition and recovery

The canonical comparison is a static headless ten-Leaf, Buffer-only, same-frame
DAG in one DeviceDomain. Every internal Flow has one writer. Runtime follows
verified frozen schedule, allocation, state, synchronization, and bindings; it
does not select candidates. Whole-composition recovery advances the device
epoch, invalidates resources/tokens/invocations, and requires dynamic rebind.

## Corpus and completion

Topology H00-H08 and frames F00-F11 are fixed by `CORPUS_V1.md`. Completion
requires positive, negative, WARP, multi-frame, Debug/Release/fresh-process,
controlled/actual recovery, and hardware evidence when an eligible adapter is
available. Performance superiority is not a completion gate.

S2-22A must end with:

```text
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```

S2-22B is not authorized by this specification.

