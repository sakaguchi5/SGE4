# Spiral 2 corpus v1

## Topologies

- H00: one-bone root
- H01: two-bone chain
- H02: eight-bone star
- H03: eight-bone chain
- H04: 15-bone balanced tree
- H05: 32-bone mixed tree
- H06: 64-bone balanced tree
- H07: 64-bone chain
- H08: 128-bone mixed tree

## Dynamic frames

- F00 identity
- F01 translations only
- F02 same-axis rotations
- F03 alternating XYZ rotations
- F04 rotation and translation
- F05 very small angles
- F06 angles near pi
- F07 fixed-seed random local Motors
- F08 deep-chain accumulated rotation
- F09 deep-chain accumulated translation
- F10 mixed-magnitude coordinates
- F11 canonical-sign boundary cases

Definitions use fixed ASCII ids, seeds, order, binary constants, Motor convention
v1, P0-P4, and Observation Contract v1. H00-H08 cross F00-F11 yields 108
qualification invocations. Fresh processes must reproduce topology, palette,
reference, and identity bytes.

