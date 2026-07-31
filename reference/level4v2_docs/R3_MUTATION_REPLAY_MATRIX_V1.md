# R3 mutation and replay matrix V1

## Contract rejection cases

1. duplicate leaf
2. duplicate endpoint
3. duplicate flow
4. unsupported resource kind
5. required endpoint left unbound
6. multiple writers in one flow
7. boundary/producer-consumer shape mismatch
8. same-frame graph cycle
9. multiple Presenters
10. mixed Target-profile identities
11. one endpoint bound to more than one flow
12. flow references an unknown endpoint
13. resigned flow identity mutation
14. resigned Contract identity mutation

## Planner-proposal rejection cases

15. allocation size mutation
16. allocation identity mutation
17. schedule mutation
18. endpoint binding mutation
19. state handoff mutation
20. signal/wait synchronization mutation
21. Presenter mutation
22. Recovery-set mutation
23. Plan identity mutation
24. Plan replay against another Contract identity

A rejection never produces `VerifiedCompositionV1`.
