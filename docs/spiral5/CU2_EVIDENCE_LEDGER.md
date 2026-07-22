# Spiral 5 CU2 evidence ledger

| Evidence | CU2 requirement |
|---|---|
| Temporal Semantic | fixed T=128, 8 bones, 4096 Work, depth 1 |
| Schedule artifact | ordered Update/Hold and exact generation sequence |
| Candidate | B.GlobalMotorHistoryReuse only |
| Dispatch authority | GPU produces hierarchy X=1 on Update and X=0 on Hold |
| History role | Global Motor state after hierarchy |
| History initialization | invalid until invocation-zero Update |
| History stability | byte-identical across Hold invocations |
| Consumer | fixed 64-group Dispatch every invocation |
| Output | CPU reference and last-update-wins identity |
| Completion | Consumer fence retains history authority on Update and Hold |
| Sidecar binding | Semantic, schedule, role, Programs, Spiral 4 indirect identity |
| Corruption | changed artifact bytes rejected |
| Legacy compatibility | Schema 17 / Runtime 17 / canonical Backend unchanged |
| Runtime policy | None |
| Debug/Release | architecture evidence byte-identical |

## Expected generated evidence

```text
build/tests/spiral5-cu2/debug.bin
build/tests/spiral5-cu2/release.bin
```

The runner prints the canonical SHA-256 after the byte-equality gate.
