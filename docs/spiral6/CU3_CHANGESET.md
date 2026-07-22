# Spiral 6 CU3 changeset

New projects:

- `157_SparseLoweringCandidate`
- `158_SparseLoweringPlanner`
- `159_SparseLoweringVerifier`
- `167_Spiral6AuthorityMutationTests`

Changed CU2 execution project:

- `156_Spiral6SparseExecution` now accepts only an opaque Verified Sparse Plan at the CU3 freeze/execute boundary and binds the Compact Index Resource to a non-zero device epoch.

No Schema 17, Runtime 17, canonical Backend or Composition Contract mutation is made. No files are deleted.
