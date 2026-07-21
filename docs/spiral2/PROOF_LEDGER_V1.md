# Spiral 2 proof ledger v1

| ID | Frozen invariant | Final status | Evidence owner |
|---|---|---|---|
| S2-I01 | Semantic has no Matrix, Queue, or Shader | Qualified | CU1 / final authority |
| S2-I02 | Bone count and topology are Frozen identity | Qualified | CU1 / CU5 |
| S2-I03 | Per-frame Motor values are outside Frozen identity | Qualified | CU1 / CU5 |
| S2-I04 | One Frozen Composition accepts different palettes | Qualified | CU5 |
| S2-I05 | A/B/C read identical Motor bytes | Qualified | CU3 / CU5 |
| S2-I06 | A has no CPU-generated Matrix Palette | Qualified | CU3 |
| S2-I07 | One Semantic generates A/B/C | Qualified | CU2 |
| S2-I08 | Candidate roles, Flow, and lowering position are fixed | Qualified | CU2 / CU3 |
| S2-I09 | Raw Candidate cannot Freeze or execute | Qualified | CU2 |
| S2-I10 | Verifier independently rederives topology | Qualified | CU2 |
| S2-I11 | Verifier independently checks Matrix position | Qualified | CU2 |
| S2-I12 | Observation schemas are identical | Qualified | CU2 / CU5 |
| S2-I13 | Every internal Flow has exactly one writer | Qualified | CU4 |
| S2-I14 | Runtime does not redecide execution facts | Qualified | CU3 / CU4 |
| S2-I15 | Dynamic input changes do not recompile | Qualified | CU5 |
| S2-I16 | Topology/count changes alter Frozen identity | Qualified | CU1 / CU5 |
| S2-I17 | WARP corpus meets CPU reference | Qualified | CU5 / Observation Contract V2 |
| S2-I18 | Pairwise candidate differences meet contract | Qualified | CU5 / Observation Contract V2 |
| S2-I19 | Same invocation is fresh-process deterministic | Qualified | CU5 |
| S2-I20 | Frozen artifacts match Debug/Release/fresh process | Qualified | CU5 / final qualification |
| S2-I21 | Recovery requires palette rebind | Qualified | CU5 |
| S2-I22 | Stale epoch Buffer/token/invocation is rejected | Qualified | CU5 |
| S2-I23 | Fair three-candidate hardware report exists | Qualified | CU6 / Measurement Evidence V2 |
| S2-I24 | No next capability before Owner selection | Qualified | Stage 22A |

All statuses were closed by executable CU evidence and the final qualification runner.
The next-capability marker remains Owner-controlled and is not changed by qualification.
