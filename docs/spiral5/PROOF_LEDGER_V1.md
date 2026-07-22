# Spiral 5 proof ledger v1

| ID | Obligation | Completion unit | CU1 status |
|---|---|---|---|
| S5-I01 | One Frozen experiment represents 128 ordered invocations | CU1/CU2 | Contract frozen |
| S5-I02 | Invocation 0 is an update and seeds history | CU1/CU2 | Contract frozen |
| S5-I03 | Generation increments only on update | CU1/CU2 | Contract frozen |
| S5-I04 | Hold input bytes equal the latest update bytes | CU1/CU5 | Contract frozen |
| S5-I05 | Temporal meaning is exact piecewise constant | CU1 | Frozen |
| S5-I06 | A/B/C preserve identical meaning and output order | CU4/CU5 | Not started |
| S5-I07 | A recomputes hierarchy and Consumer every invocation | CU4 | Not started |
| S5-I08 | B reuses Global Motor history on hold | CU2/CU4 | Not started |
| S5-I09 | C retains final output on hold | CU4 | Not started |
| S5-I10 | Update/hold Dispatch rules are independently rederived | CU2/CU3 | Not started |
| S5-I11 | Runtime and Backend make no temporal policy decision | CU2–CU6 | Contract frozen |
| S5-I12 | Raw Candidates cannot execute or Freeze | CU3 | Not started |
| S5-I13 | History binds role, generation, epoch, Target, Resource | CU3/CU5 | Not started |
| S5-I14 | Stale and cross-context history is rejected | CU3/CU5 | Not started |
| S5-I15 | Recovery invalidates and explicitly reseeds history | CU5 | Not started |
| S5-I16 | Fresh Debug/Release architecture evidence is deterministic | CU5 | Not started |
| S5-I17 | Real-GPU evidence separates temporal cost components | CU6 | Not started |
| S5-I18 | Measurement does not authorize Runtime selection | CU6 | Contract frozen |

## CU1 conclusion

CU1 freezes the research question, exact temporal meaning, schedules, Candidate roles, and authority boundaries.

CU1 does not prove that the current ABI is insufficient and does not authorize a temporal extension. That proof belongs to CU2.
