# Spiral 7 CU6 Evidence Ledger

| Evidence | Required result |
|---|---|
| CU5 Architecture binding | `1F1D09B4...CB5AB73` |
| CU5 Controlled Recovery binding | `7F0247B1...16CFF62B` |
| CU5 Fresh-process binding | `091EAEC0...A6F5A92` |
| Format self-test | Debug/Release synthetic evidence byte-identical |
| Corruption rejection | flipped and truncated evidence rejected |
| Candidate orders | all six A/B/C permutations, unique and balanced |
| Real adapter | non-software D3D12 hardware adapter |
| Correctness precheck | all 160 cases per run; A/B/C outputs byte-identical |
| Write authority | A=entire universe; B/C=exactly `T_t` |
| Grid | 4 patterns x 5 Active counts x 8 Transition counts |
| Exact case identity | no silent clipping of illegal-looking `(A,T)` pairs |
| Timestamp source | D3D12 Timestamp Query / queue frequency recorded |
| Binary evidence schema | `S7M2`; per-sample raw ticks, effective iterations and censor flag |
| Zero-dispatch resolution | 0 tick is accepted only for 0 dispatch and recorded as a one-tick conservative upper bound |
| Nonzero-dispatch resolution | whole balanced order retries with doubled iterations, ceiling 65536 |
| Drift control | fixed A control before/after; excessive blocks rejected |
| Primary metric | GPU batch nanoseconds per effective iteration |
| Decision report | ranking, paired ratios, T crossover, A crossover, Pattern dependence |
| Evidence reload | binary evidence independently parsed and report reproduced byte-identically |
| Policy boundary | Runtime Candidate policy remains `None` |
| Completion | `SPIRAL 7 EXPERIMENT COMPLETE` |
| Closure | Owner required |

Evidence output directory:

```text
build/tests/spiral7-cu6/
  self-test-debug.bin
  self-test-release.bin
  measurement_evidence_v2.bin
  decision_cases_v2.csv
  decision_report_v2.txt
  decision_report_reloaded_v2.txt
```
