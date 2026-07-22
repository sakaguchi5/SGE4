# Spiral 4 proof ledger v1

| ID | Proof obligation | First owning CU | Current status after CU2 |
|---|---|---:|---|
| S4-I01 | Runtime `Nf` changes without changing Frozen structure | CU2 | Qualified by single-indirect artifact/executor |
| S4-I02 | `0 <= Nf <= 4096`, including zero and full | CU2 | Qualified for boundary construction and WARP cases |
| S4-I03 | Active prefix exact; inactive tail untouched | CU5 | CU2 architectural subset qualified |
| S4-I04 | Representation path fixed, not policy-authorized | CU1 | Frozen |
| S4-I05 | All Candidates preserve identical meaning/output order | CU4 | Minimal WARP equivalence qualified; full corpus remains CU5 |
| S4-I06 | Raw Candidate cannot Freeze/execute | CU3 | Qualified by compile-time type boundaries |
| S4-I07 | Planner-independent Verifier rederives all indirect decisions | CU3 | Qualified for Single ExecuteIndirect |
| S4-I08 | Runtime performs no Active Count readback decision | CU2 | Qualified by GPU Argument Producer path |
| S4-I09 | Backend maps explicit Frozen indirect Operation mechanically | CU2 | CU3 binds mechanical sidecar execution to Verified authority |
| S4-I10 | Batch partition is complete, disjoint, ordered | CU4 | Qualified for 5 sizes x 19 Active Counts |
| S4-I11 | V1 fixed Batch slots; no indirect count Buffer | CU4 | Implemented and GPU-observed |
| S4-I12 | ABI extension is minimal, versioned, evidence-justified | CU2 | Sidecar V1 qualified; final canonical integration deferred |
| S4-I13 | Active output/reference and inactive sentinel qualify | CU5 | CU2 architectural subset qualified |
| S4-I14 | Debug/Release/fresh-process artifacts are byte-identical | CU5 | CU2 Debug/Release artifact gate included |
| S4-I15 | Recovery regenerates arguments and rejects stale epochs | CU5 | Not started |
| S4-I16 | Hardware evidence binds all identities and variables | CU6 | Not started |
| S4-I17 | Winner/crossover is not a completion requirement | CU1 | Frozen |
| S4-I18 | Runtime policy authorization remains none | CU6 | Preserved |
| S4-I19 | Spiral 3 evidence remains immutable history | CU1 | Preserved |
| S4-I20 | Next capability remains Owner-only | CU6 | Preserved |

“Qualified in sidecar executor” does not replace CU3 independent authority or CU5 whole-composition qualification.


## CU3 authority evidence

The Single ExecuteIndirect path now has a distinct Raw proposal, an independent
Verifier, an opaque Verified Plan, a context-bound certificate, and an actual
sidecar/shader execution binding.

S4-I05 remains open because Fixed Maximum and Batched candidates are CU4.
S4-I15 remains open because device-epoch and Recovery authority are CU5.


## CU4 evidence

Seven Candidate identities are now verified from one Canonical Semantic.
CU4 proves the Candidate family and Batch structure, but does not declare
Architecture Complete. CU5 still owns the complete WARP corpus, deterministic
fresh-process artifacts, Recovery, and stale-epoch rejection.
