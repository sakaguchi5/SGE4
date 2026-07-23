# Spiral 7 CU4 corpus V1

The CU4 corpus is one chained eighteen-invocation timeline. Every invocation executes A, B and C, for 54 WARP Candidate executions.

```text
InitialPrefix64
HoldPrefix64
DirtyOnePrefix64
Dirty32Prefix64
ActivatePrefix65
DeactivatePrefix32
BalancedChurnUniform32
PatternMigrationHash32
ActivateBlock256
HoldBlock256
Dirty64Block256
ExpandPrefix1024
MigrateHash1024
HoldHash1024
EmptyReset
FullActivate4096
Dirty1024Full
DeactivateHash4095
```

The corpus crosses empty, hold, local dirty, activation, deactivation, balanced churn, pattern migration, block clustering, full activation and near-full deactivation boundaries.
