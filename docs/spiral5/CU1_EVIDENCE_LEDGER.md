# Spiral 5 CU1 evidence ledger

| Evidence | Required CU1 result |
|---|---|
| Owner selection | Temporal State Flow and Verified History Reuse Lowering |
| Baseline | Spiral 4 Experiment Complete commit |
| Meaning | exact piecewise-constant, last-update-wins |
| Timeline | 128 invocations; frame 0 update |
| Primary variable | external update interval U |
| Measurement U corpus | 1,2,4,8,16,32,64 |
| Candidate A | every-invocation hierarchy and Consumer recompute |
| Candidate B | Global Motor history reuse |
| Candidate C | final output history reuse |
| History | depth 1; role/generation/epoch bound |
| Interpolation | explicitly not in V1 |
| Runtime policy | unauthorized |
| ABI mutation | none in CU1 |
| Implementation | documents and verification only |

## Baseline evidence retained

Spiral 4’s formal state remains present:

```text
Architecture Complete
Experiment Complete
RuntimePolicyAuthorization: None
```

The Owner’s Temporal selection resolves Spiral 4’s deferred next-capability gate but does not authorize a Spiral 5 Runtime policy or implementation shortcut.
