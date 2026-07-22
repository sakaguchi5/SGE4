# Spiral 4 V1 non-goals

Spiral 4 V1 studies a runtime-varying active prefix inside one fixed-capacity Compute workload.

It does not introduce:

- variable Semantic Work-node count,
- variable Leaf, Flow, Package, or Resource count,
- dynamic graph mutation,
- Buffer allocation or capacity changes per frame,
- sparse active-index lists,
- GPU culling, compaction, prefix sum, scan, or stream compaction,
- an indirect count Buffer,
- arbitrary ExecuteIndirect command types,
- DrawIndirect, indexed drawing, or mixed command signatures,
- Temporal Flow or history,
- Texture Flow, Raster, Present, or image comparison,
- multiple Consumer kinds,
- heterogeneous per-record cost,
- Matrix/PGA representation competition,
- Runtime adaptive Candidate selection,
- Conditional Region or Verified Variant Set,
- partial Recovery, multiple DeviceDomain, multiple Adapter, streaming, or residency,
- a universal performance rule for direct, indirect, or Batched dispatch.

No empty future abstraction, capability bit, placeholder Operation, or unused project is created in CU1.
