# Spiral 7 CU4 evidence ledger

The CU4 evidence bundle contains:

- all A/B/C Frozen family Artifacts for eighteen invocations;
- all opaque Verified Family Candidate certificates;
- the WARP Candidate-family observation report;
- the Raw mutation count;
- a final SHA-256 digest over the evidence body.

Debug and Release must emit byte-identical bundles. The Owner-run evidence digest is recorded only after the gate succeeds; this package does not invent that digest.
