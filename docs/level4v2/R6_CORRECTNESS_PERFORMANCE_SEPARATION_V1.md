# R6 correctness and performance separation V1

## Correctness gate

The R6 correctness gate consumes only deterministic identities, terminal migration outcomes, representative/adversarial v2 authority cases, and accepted R1-R5 qualification digests.

It does not parse timing samples, rank Candidates, choose a universal winner, or mutate Runtime policy.

Every correctness case must use `BindingKindV1::V2Authority`.

## Observational Evidence

The accepted external bundle remains:

```text
spiral7-cu6-2.zip
SHA-256 = 9ee852c2a2574a8cb9c163b6e94213b855317bba74319e6443e9aa98c10c24d9
Tracked by Git = No
```

The paired Decision report remains:

```text
combined_paired_decision_report_v3.txt
SHA-256 = b840cb1425f5e45ccac2f0c84750584313350458062dab98f8552290e8bfda81
```

These identities are represented in the `ObservationalPerformance` lane and must use `BindingKindV1::RetainedObservationalEvidence`. They cannot be substituted for a v2 authority identity. A lane or binding-kind mismatch is a certificate rejection. `RuntimeCandidatePolicyAuthorization` remains `None`.
