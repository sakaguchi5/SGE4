# Level 4 v2 R6 Migration corpus

## 1. Purpose

R6 is the migration-proof stage of Level 4 v2. It adds no GPU capability and no Runtime policy. It binds the retained Level 4 v1 and Spiral 1-7 proof lineage to the R1-R5 Canonical authorities and records a terminal migration outcome for every R0 carried Invariant.

The legal chain remains:

```text
retained source identity
  + accepted R1-R5 deterministic Evidence
  + representative/adversarial v2 authority case
        -> validated Migration Corpus
        -> opaque Migration Certificate
```

A source document or prior executable test cannot mint a v2 authority. Correctness cases must bind to an identity produced through R1-R5 using `V2Authority`. Observational cases bind instead to `RetainedObservationalEvidence`; an observational measurement is never represented as a v2 execution authority.

## 2. Coverage

```text
Frozen source lineages        16
R0 carried Invariants         40
Terminal outcomes             40
Migration cases               26
  correctness                 24
  observational performance    2
Certificate rejection cases   20
```

Every R0 Invariant has the terminal outcome `Reproduced`. No Invariant is silently inherited, omitted, or retired.

## 3. Representative v2 authority path

The executable corpus constructs and binds:

- an R2 Verified-only Frozen leaf authority;
- an R3 Verified-only Frozen finite Buffer DAG composition;
- an R4 initial full-active Frozen Dynamic Invocation and Verified History;
- an R5 Frozen-only materialization and submission;
- controlled whole-composition Recovery;
- explicit post-Recovery external rebind;
- an R4 `RecoverySeed` invocation;
- forced removal, removed-Adapter exclusion and explicit reacquisition.

The resulting identities are used by the Migration cases. The migration layer does not reconstruct Candidate, Composition, Dynamic or Runtime decisions.

## 4. Correctness and observational performance

Correctness Evidence and observational performance Evidence have different lanes.

```text
Correctness lane
  R1-R5 deterministic Evidence
  architecture and WARP qualification
  migration-certificate validity

Observational lane
  external Spiral 7 CU6 bundle
  paired Decision Map
```

The external bundle remains outside Git and is retained by SHA-256. Its measurements cannot authorize a Runtime Candidate policy, modify the Canonical ABI, or turn an observed winner into a universal winner.

Binding is lane-specific:

```text
Correctness              -> V2Authority
ObservationalPerformance -> RetainedObservationalEvidence
```

## 5. Reference retention

R6 proves that all 40 migration outcomes exist. It does not delete, move or archive any retained reference Project. Physical retirement remains an explicit R7 Owner decision after the R6 gate is accepted.
