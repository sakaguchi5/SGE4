# Spiral 5 proof ledger v1

| ID | Obligation | Completion unit | Current status after CU2 |
|---|---|---|---|
| S5-I01 | One Frozen experiment represents 128 ordered invocations | CU1/CU2 | Proved for CU2 schedule artifacts |
| S5-I02 | Invocation 0 is an update and seeds history | CU1/CU2 | Enforced and negative-tested |
| S5-I03 | Generation increments only on update | CU1/CU2 | Enforced and mutation-tested |
| S5-I04 | Hold input bytes equal the latest update bytes | CU1/CU5 | Canonical generation selection implemented; full external-input gate deferred |
| S5-I05 | Temporal meaning is exact piecewise constant | CU1 | Frozen |
| S5-I06 | A/B/C preserve identical meaning and output order | CU4/CU5 | Not started |
| S5-I07 | A recomputes hierarchy and Consumer every invocation | CU4 | Not started |
| S5-I08 | B reuses Global Motor history on hold | CU2/CU4 | Proved for P1/P4/P64/Irregular WARP architecture |
| S5-I09 | C retains final output on hold | CU4 | Not started |
| S5-I10 | Update/hold Dispatch rules are independently rederived | CU2/CU3 | Canonical artifact and GPU observation implemented; independent Verifier deferred |
| S5-I11 | Runtime and Backend make no temporal policy decision | CU2–CU6 | CU2 mechanical mapping proved |
| S5-I12 | Raw Candidates cannot execute or Freeze | CU3 | Not started |
| S5-I13 | History binds role, generation, epoch, Target, Resource | CU3/CU5 | Role/generation sidecar fields implemented; full binding deferred |
| S5-I14 | Stale and cross-context history is rejected | CU3/CU5 | Not started |
| S5-I15 | Recovery invalidates and explicitly reseeds history | CU5 | Policy frozen in CU2 artifact; execution deferred |
| S5-I16 | Fresh Debug/Release architecture evidence is deterministic | CU5 | Preliminary CU2 Debug/Release gate implemented |
| S5-I17 | Real-GPU evidence separates temporal cost components | CU6 | Not started |
| S5-I18 | Measurement does not authorize Runtime selection | CU6 | Contract frozen |

## CU2 conclusion

The Package ABI already contains physical temporal-resource machinery. The missing authority is at the Composition/Semantic boundary. CU2 supplies a versioned sidecar and proves one Global Motor history path without mutating legacy ABI or returning Update/Hold decisions to Runtime.
