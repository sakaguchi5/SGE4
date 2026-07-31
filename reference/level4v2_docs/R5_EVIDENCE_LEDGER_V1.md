# R5 Evidence ledger V1

| Evidence | Requirement |
|---|---|
| Canonical Debug/Release bytes | identical; SHA-256 `40d78dc35894a55e50b314bd0afcbb60966f92e942ac70856155964cd8e66498` |
| Canonical scenarios | 10 |
| Canonical rejection observations | 12 |
| Windows Debug/Release bytes | identical; SHA-256 `1e4adaa04dc8d1d235f8f3438f4295b09b36480174f6a7d9fa0fbaba79ea7a9a` |
| WARP controlled rebuild | epoch advances, Frozen Composition revalidated, all objects released/rematerialized |
| stale epoch | prior resource/token rejected |
| actual removal | WARP RemoveDevice path enters AwaitingAdapter |
| removed Adapter | excluded on retry; AwaitingAdapter preserved |

The Windows result proves retained Backend primitives. R6 must separately prove corpus-level binding from v2 Composition/Dynamic authorities to executable retained artifacts.
