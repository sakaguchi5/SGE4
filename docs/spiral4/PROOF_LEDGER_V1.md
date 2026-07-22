# Spiral 4 proof ledger v1

| ID | Proof obligation | First owning CU | CU1 status |
|---|---|---:|---|
| S4-I01 | Runtime `Nf` changes without changing Frozen structure | CU2 | Contract frozen |
| S4-I02 | `0 <= Nf <= 4096`, including zero and full | CU2 | Contract frozen |
| S4-I03 | Active prefix exact; inactive tail untouched | CU5 | Corpus frozen |
| S4-I04 | Representation path fixed, not policy-authorized | CU1 | Frozen |
| S4-I05 | All Candidates preserve identical meaning/output order | CU4 | Contract frozen |
| S4-I06 | Raw Candidate cannot Freeze/execute | CU3 | Contract frozen |
| S4-I07 | Planner-independent Verifier rederives all indirect decisions | CU3 | Contract frozen |
| S4-I08 | Runtime performs no Active Count readback decision | CU2 | Boundary frozen |
| S4-I09 | Backend maps explicit Frozen indirect Operation mechanically | CU2 | Boundary frozen |
| S4-I10 | Batch partition is complete, disjoint, ordered | CU4 | Formula frozen |
| S4-I11 | V1 fixed Batch slots; no indirect count Buffer | CU4 | Frozen |
| S4-I12 | ABI extension is minimal, versioned, evidence-justified | CU2 | Policy frozen |
| S4-I13 | Active output/reference and inactive sentinel qualify | CU5 | Contract frozen |
| S4-I14 | Debug/Release/fresh-process bytes match | CU5 | Contract frozen |
| S4-I15 | Recovery regenerates arguments and rejects stale epochs | CU5 | Contract frozen |
| S4-I16 | Hardware evidence binds all identities and variables | CU6 | Contract frozen |
| S4-I17 | Winner/crossover is not a completion requirement | CU1 | Frozen |
| S4-I18 | Runtime policy authorization remains none | CU6 | Frozen |
| S4-I19 | Spiral 3 evidence remains immutable history | CU1 | Frozen |
| S4-I20 | Next capability remains Owner-only | CU6 | Frozen |

Proof status may only move from unimplemented to qualified. An invariant may not be deleted merely because implementation is reconstructed.
