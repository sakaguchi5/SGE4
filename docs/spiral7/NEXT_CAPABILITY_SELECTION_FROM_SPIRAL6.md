# Next capability selection from Spiral 6

## Owner decision

At baseline commit:

```text
d0bb0d406fca8beabed2331daff870ea414dd87d
```

the Owner records that the Spiral 6 CU5 architecture qualification and CU6 V2 real-GPU measurement completed successfully outside this document update.

The selected next capability is:

```text
Versioned Sparse Delta Flow and Verified Incremental History Lowering
```

Japanese title:

```text
世代付き疎差分Flowと増分履歴再利用の検証済みLowering
```

This is an Owner capability selection. It is not a Runtime Candidate-selection authorization.

```text
SGE4-5 SPIRAL 6 CLOSED
RuntimePolicyAuthorization = None
```

## Why this follows Spiral 6

Spiral 4 verified dynamic active quantity and indirect dispatch. Spiral 5 verified temporal history reuse while the Work set remained complete. Spiral 6 verified one exact non-prefix Work set while the temporal dimension remained frozen to one invocation.

The missing Level 4 v2 integration boundary is therefore not another isolated dimension. It is the exact relation among:

```text
current exact Active Set
previous exact Active Set
modified surviving members
activation members
deactivation members
retained valid history
exact update / clear execution
```

Spiral 7 is selected to prove that relation before Level 4 v2 Canonical reconstruction.

## Frozen sequencing decision

```text
Spiral 7
  -> prove Sparse x Temporal x Indirect authority
  -> close Spiral 7
  -> stop exploratory Spiral growth
  -> reconstruct Level 4 v2 Canonical Composition
```

Texture Flow, Conditional Region, Residency, Partial Recovery and multiple DeviceDomains remain outside Spiral 7.
