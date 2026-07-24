# R4 mutation and replay matrix V1

## Exact-set construction

1. zero universe rejected;
2. out-of-range member rejected;
3. duplicate member rejected.

## Request and History authority

4. Request identity mutation;
5. modified set on Initial/Recovery seed;
6. Initial seed carrying previous History;
7. Continue missing previous History;
8. Recovery seed carrying previous History;
9. cross-Composition History replay;
10. cross-Semantic History replay;
11. stale Device-epoch History replay;
12. History universe mismatch;
13. non-successor timeline ordinal;
14. modified member outside current Active set;
15. modified newly activated member rather than survivor.

## Planner proposal mutation

16. request identity;
17. previous Active set;
18. activation set;
19. deactivation set;
20. update set;
21. retain set;
22. transition set;
23. per-item generation vector;
24. transition record action;
25. indirect work count;
26. exact dynamic write-set identity;
27. Dynamic decision identity.

Every rejection returns no `VerifiedDynamicInvocationV1`.
