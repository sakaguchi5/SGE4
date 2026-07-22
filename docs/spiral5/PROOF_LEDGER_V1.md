# Spiral 5 proof ledger v1

| ID | Obligation | Completion unit | Current status after CU3 |
|---|---|---|---|
| S5-I01 | One Frozen experiment represents 128 ordered invocations | CU1/CU2 | Proved for CU2/CU3 schedule artifacts |
| S5-I02 | Invocation 0 is an update and seeds history | CU1/CU2 | Enforced and negative-tested |
| S5-I03 | Generation increments only on update | CU1–CU3 | Semantic validation plus independently hashed invocation authority |
| S5-I04 | Hold input bytes equal the latest update bytes | CU1/CU5 | Canonical generation selection implemented; full external-input gate deferred |
| S5-I05 | Temporal meaning is exact piecewise constant | CU1 | Frozen |
| S5-I06 | A/B/C preserve identical meaning and output order | CU4/CU5 | Not started |
| S5-I07 | A recomputes hierarchy and Consumer every invocation | CU4 | Not started |
| S5-I08 | B reuses Global Motor history on hold | CU2/CU3 | Verified P4 WARP route plus CU2 schedule corpus |
| S5-I09 | C retains final output on hold | CU4 | Not started |
| S5-I10 | Update/hold Dispatch rules are independently rederived | CU2/CU3 | Proved by Planner-independent Verifier |
| S5-I11 | Runtime and Backend make no temporal policy decision | CU2–CU6 | Verified route remains mechanical |
| S5-I12 | Raw Candidates cannot execute or Freeze | CU3 | Compile-time and runtime gates proved |
| S5-I13 | History binds role, generation, epoch, Target, Resource | CU3/CU5 | Frozen identity binds all listed fields; Recovery behavior deferred |
| S5-I14 | Stale and cross-context history is rejected | CU3/CU5 | Cross-schedule/context/resource/epoch identity replay rejected; native stale objects deferred |
| S5-I15 | Recovery invalidates and explicitly reseeds history | CU5 | Policy frozen; execution deferred |
| S5-I16 | Fresh Debug/Release architecture evidence is deterministic | CU5 | CU2 architecture and CU3 authority bundle equality gates implemented |
| S5-I17 | Real-GPU evidence separates temporal cost components | CU6 | Not started |
| S5-I18 | Measurement does not authorize Runtime selection | CU6 | Contract frozen |

## CU3 conclusion

One `GlobalMotorHistoryReuse` path now has an independent authority chain. The Planner proposes but cannot certify. The Verifier independently reconstructs schedule generations, physical Dispatch rules, the CU2 artifact, Program identities, and the expected History Resource identity. Execution requires an opaque Verified Plan and an epoch-bound actual Resource binding.
