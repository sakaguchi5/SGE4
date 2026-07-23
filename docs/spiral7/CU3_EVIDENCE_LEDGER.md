# Spiral 7 CU3 evidence ledger

| Evidence | Boundary |
|---|---|
| Raw Candidate bytes | Complete untrusted proposal, including identity and policy fields |
| Verified Delta Lowering bytes | Independently reconstructed plan and opaque certificate |
| Frozen Verified execution bytes | Verified plan, Artifact, three resource bindings and Device epoch |
| Verified WARP report bytes | One exact Transition execution using the Frozen entry point |
| Mutation counters | 50 Raw attacks, 4 Verified replay gates, 14 resource/artifact/epoch gates |
| Final digest | SHA-256 over the complete CU3 evidence payload |

CU3 proves authority separation and replay rejection for the Compact Delta Candidate. It does not prove Candidate-family equivalence, Recovery or a Runtime selection policy.
