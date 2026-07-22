# SGE4-5 Spiral 6

Spiral 6 studies exact non-prefix sparse Work membership.

```text
fixed 4096-record universe
+ externally verified exact set S
+ three equivalent physical representations
```

Candidate family:

```text
A. DenseMembershipMaskPredicate
B. CompactIndexListDispatch
C. ActiveBlockListLocalMask
```

The Semantic set, output order and inactive sentinel are fixed. Only sparse representation and execution granularity vary.

Current path:

```text
CU1 research contract                         PASSED
CU2 Compact Index architecture                PASSED
CU3 independent Sparse authority              PASSED
CU4 Sparse Candidate family                   PASSED
CU5 Architecture qualification                supplied
CU6 real-GPU measurement and Decision Report
```

CU5 preparation and runner:

```text
run_sge4_5_spiral6_cu5_prepare.bat
run_sge4_5_spiral6_cu5_architecture_qualification.bat
```

CU5 qualifies 21 cardinalities × 4 exact-set patterns × 3 Candidates. It requires exact writes, inactive sentinel preservation, full-output byte identity, fresh-process determinism, Controlled Recovery, actual Device Removal, and explicit Sparse set rebind plus Derived Representation rebuild.

A successful runner declares `SGE4-5 SPIRAL 6 ARCHITECTURE COMPLETE`. Runtime Sparse policy remains unauthorized.

## CU6

Prepare the two measurement projects:

```text
run_sge4_5_spiral6_cu6_prepare.bat
```

Run deterministic self-tests, the real-GPU measurement and Decision Report:

```text
run_sge4_5_spiral6_cu6_measurement_decision_evidence.bat
```
