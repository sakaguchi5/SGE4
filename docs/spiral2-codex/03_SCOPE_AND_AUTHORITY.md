# Scope and authority summary

The complete details are in `reference/SPIRAL2_ROADMAP_EXTRACTED.md`.

## Frozen structure

The following belong to Semantic or Frozen identity:

- Bone count
- Parent index table
- Root identity
- Canonical bone order
- Depth layers
- Motor convention version
- Dynamic palette schema
- Probe schema
- Output schema
- Observation Contract
- Candidate graph kind
- Program template versions
- Target profile

Bone count or topology changes require recompilation.

## Dynamic values

The following do not belong to Frozen identity:

- Local Motor Palette values
- Frame number
- dynamic reference output
- invocation identity

Different palettes must be submitted to the same Frozen Composition without recompilation.

## Restricted hierarchy

- one root
- fixed bone count
- fixed parents
- `parentIndex < childIndex`
- finite acyclic tree
- rigid transforms only
- no scale
- same-frame only

## Three candidates

### A. Matrix hierarchy

```text
Motor input
-> LocalMotorToMatrix
-> MatrixHierarchy
-> MatrixProbeApply
```

Matrix materialization occurs before hierarchy composition. Intermediate palette stride is 48 bytes per bone.

### B. Direct PGA hierarchy

```text
Motor input
-> MotorHierarchy
-> DirectPgaProbeApply
```

No Matrix materialization. Intermediate palette stride is 32 bytes per bone.

### C. Hybrid hierarchy

```text
Motor input
-> MotorHierarchy
-> GlobalMotorToMatrix
-> MatrixProbeApply
```

Matrix materialization occurs only after hierarchy composition.

## Fairness

All candidates consume identical canonical binary32 Motor bytes.

It is forbidden to generate a Matrix Palette on the CPU only for Candidate A.

Use the same canonical depth layers, bone order, and dispatch partition unless evidence explicitly requires a separately controlled experiment.

## Common observation

Apply each bone's global rigid transform to fixed probe points:

```text
P0 = (0, 0, 0, 1)
P1 = (1, 0, 0, 1)
P2 = (0, 1, 0, 1)
P3 = (0, 0, 1, 1)
P4 = (0.37, -0.61, 1.13, 1)
```

Record reference error, pairwise error, ULP distance, non-finite values, rigidity, orthogonality, orientation, and translation.

CPU reference must be binary64 and independent of the three GPU candidate implementations.

## Authority chain

```text
Hierarchy experiment specification
-> Canonical Hierarchy Semantic
-> Raw Candidate Graph
-> Independent Hierarchy Verifier
-> Verified Representation Graph
-> Leaf Compiler
-> Frozen Leaf Packages
-> Level 4 Composition Contract
-> Composition Planner
-> Independent Composition Verifier
-> Frozen Composition
-> Runtime / Backend
```

Raw candidate graphs cannot Freeze, execute, or enter Composition.

The representation Verifier must independently rederive:

- topology and depth layers
- Leaf role sequence
- Flow graph
- Matrix materialization position
- dynamic schema
- intermediate stride
- Program template digest
- binding digest
- observation schema

It must not call the Planner implementation.

## Non-goals

Do not implement:

- skinning or vertex influence
- animation clips, blending, IK, constraints
- temporal history or previous-frame pose
- motion vectors
- variable bone count or topology
- DispatchIndirect
- variable-length buffers
- runtime variant selection
- conditional regions
- Texture Flow
- streaming/residency
- multiple devices/adapters
- partial recovery
- general PGA multivectors
- arbitrary shader theorem proving
- future placeholders or empty capability abstractions

Performance superiority is not a completion gate.
