# R5 authority ownership V1

| Fact | Owner | Runtime action |
|---|---|---|
| Composition graph, allocation, schedule, state and synchronization | R3 Frozen Composition | consume identity and materialize; never reinterpret |
| Membership, transition, write set and indirect quantity | R4 Frozen Dynamic Invocation | submit exactly the verified values |
| Adapter candidate order | external caller | filter removed identities; do not rank by performance |
| Device epoch | R5 Loaded Runtime | advance only after successful reacquisition/materialization |
| External state | external owner | explicitly rebind for the current Composition identity and epoch |
| Accepted History | R4 Verified History plus R5 current epoch | reject stale epoch and invalidate on Recovery |
| Native objects | Backend implementation | create and destroy under R5 lifecycle control |

Single-fact rule: R5 stores typed identities and lifecycle state. It does not duplicate Semantic, Composition or Dynamic decision bodies.
