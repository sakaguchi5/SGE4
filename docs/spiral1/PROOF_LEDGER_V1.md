# Spiral 1 Proof Ledger V1

- Contract ID: `SGE4-5.Spiral1.ProofLedger.V1`
- Status: Frozen schema; implementation evidence accumulates monotonically

実装、ABI、C++ APIは再構築できるが、証明済みInvariantを理由なく削除してはならない。Statusは`Pending`, `Qualified`, `Rejected`, `Superseded-with-reason`のいずれかとする。

| ID | Invariant | Owner Stage | Positive Evidence | Negative Mutation | Status |
|---|---|---|---|---|---|
| S1-I01 | Canonical SemanticにCartesian Matrixが存在しない | Stage 04 | Semantic model inspection / serialization | Matrix field injection | Pending |
| S1-I02 | 同じSemantic identityから二Lowering候補を生成できる | Stage 06 | Candidate planning test | semantic identity divergence | Pending |
| S1-I03 | Raw CandidateはFreezeも実行もできない | Stage 07 | authority compile-time/runtime gate | raw direct freeze | Pending |
| S1-I04 | VerifierがMatrix定数を独立再導出する | Stage 07 | independent bytes equality | matrix coefficient mutation | Pending |
| S1-I05 | VerifierがDirect PGA係数を独立再導出する | Stage 07 | independent bytes equality | motor coefficient/order mutation | Pending |
| S1-I06 | Versioned Canonical Program Templateのみ受理 | Stage 07 | allowlist digest check | shader/template substitution | Pending |
| S1-I07 | 二候補のI/O schemaとObservation Contractが同一 | Stage 06-07 | candidate/verifier schema check | stride/count/contract swap | Pending |
| S1-I08 | Level 4 v1 Buffer Flowだけで比較Compositionを構成 | Stage 10 | four-leaf composition | unapproved Level 4 extension | Pending |
| S1-I09 | 各internal Flowはexactly one writer | Stage 10 | composition verifier | second writer mutation | Pending |
| S1-I10 | Runtimeはlowering/schedule/state/syncを再判断しない | Stage 10-12 | frozen-only runtime evidence | runtime reselection path | Pending |
| S1-I11 | S00-S15がCPU reference誤差契約を満たす | Stage 11 | WARP corpus report | candidate output corruption | Pending |
| S1-I12 | MatrixとDirect PGA相互差が誤差契約を満たす | Stage 11 | pairwise report | paired output mutation | Pending |
| S1-I13 | Comparison集計はpoint順に決定的 | Stage 05/11 | fresh-process readback/report | reordered reduction | Pending |
| S1-I14 | Frozen成果物がDebug/Release/fresh processでbyte-identical | Stage 12 | manifest byte comparison | timestamp/process value injection | Pending |
| S1-I15 | Controlled recoveryとactual removal後に同じ観測を再現 | Stage 12 | recovery report | stale instance reuse | Pending |
| S1-I16 | stale epoch object/tokenを拒否 | Stage 12 | epoch negative tests | old token/resource injection | Pending |
| S1-I17 | 性能結果に関係なく公平な比較reportを生成 | Stage 13 | ABBA benchmark report | unequal corpus/profile | Pending |
| S1-I18 | 次のLevel 4拡張はDecision Report証拠なしに追加されない | Stage 14 | decision report review | speculative capability addition | Pending |

## Stage 03 evidence

Stage 03は上記Invariantの意味、owner、必要証拠、negative mutationを固定する。実装証拠はStage 04以降で追加する。Stage 03自身の証拠はResearch Contract filesのdigest、F0 code baseline維持、contract verifier合格である。
