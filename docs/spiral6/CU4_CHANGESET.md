# Spiral 6 CU4 changeset

## New projects

- `160_SparseCandidateFamily`
- `161_SparseCandidateFamilyPlanner`
- `162_SparseCandidateFamilyVerifier`
- `163_Spiral6SparseFamilyExecution`
- `168_Spiral6SparseCandidateFamilyTests`

## New capability

CU4 adds the three-Candidate Sparse representation family:

1. Dense 4096-bit membership mask;
2. CU2-compatible fixed-capacity Compact Index List;
3. Active Block List with one 64-bit local mask per active block.

All Candidates are independently Verified, Frozen to actual representation Resource identities and bound to a non-zero device epoch before WARP execution.

## Unchanged boundaries

CU4 does not modify D3D12 Schema 17, Runtime 17, canonical Backend, Composition Contract, Spiral 4 indirect machinery, Spiral 5 Temporal extension or the CU2/CU3 files.

No files are deleted.
