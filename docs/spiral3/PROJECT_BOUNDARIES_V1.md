# Spiral 3 project boundaries V1

## Semantic and planning

```text
100 ReuseSweepSemantic
101 Spiral3Contracts
102 Spiral3Corpus
103 ReuseRepresentationCandidate
104 ReuseRepresentationPlanner
105 ReuseRepresentationVerifier
```

The Semantic layer contains no Matrix/Backend policy. Raw candidates cannot construct the opaque Verified type. The Verifier has no Planner reference.

## Frozen Level 4 lowering

```text
106 Spiral3LeafCompiler
107 Spiral3Scenarios
```

Only Verified Representation enters the Leaf Compiler. `107` freezes the eleven-Leaf/twelve-Flow comparison and remains Runtime/Backend-free.

## Execution and evidence

```text
108 Spiral3Execution
109 Spiral3PerformanceExperiment
115 WarpObservationTests
116 RecoveryTests
117 FreezeTests
118 PerformanceTests
119 Launcher
```

`108` owns WARP/runtime observation. `109` owns real-hardware measurement, binary evidence and report generation. No CU6 concept is added to Package Runtime, D3D12 Backend or Composition Runtime. Schema 17 and Runtime 17 remain unchanged.
