# Spiral 7 CU6 — Real-GPU Measurement

## 1. CU6の目的

CU5までで、次のArchitecture上の正しさは完成している。

```text
A = FullActiveDenseRecompute
B = CompactDeltaIndexHistoryReuse
C = AffectedBlockDeltaHistoryReuse

A/B/Cの最終4096-record出力は各Invocation後にbyte-identical
B/Cの書込み集合は正確にT_t
H_tは書き込まれず、有効なPrevious Historyを保持
Recovery後は旧epoch Historyを拒否し、W_t=A_t、H_t=emptyで再構築
```

CU6はこの正しさを作り直す段階ではない。完成したArchitectureを固定したまま、実機GPU上でA/B/Cの相対性能がどの入力領域で変化するかを観測する段階である。

したがってCU6の研究質問は次である。

> 同じ意味結果を生成するA/B/Cについて、Active数、物理Transition数、影響Block数、Sparse配置、Update/Clear構成が変わると、観測上の相対的な有利不利はどこで変化するか。

## 2. CU6が行わないこと

CU6は以下を行わない。

- Architecture、Semantic、Verifier、Recovery契約の変更
- RuntimeまたはBackendによるA/B/C自動選択
- 一台のGPUで得た結果からの普遍的勝者の宣言
- ゲーム側ポリシー、Hardware一般、最終操作の推論
- Level 4 v2 Canonical ABIへの即時統合

測定結果はAdapter、Driver、Shader bytecode、測定Profileに束縛された観測証拠である。

## 3. 測定変数

固定するもの:

```text
Work universe = 4096
Threads/group = 64
Direct PGA consumer semantics
Output order = universe-index order
History depth = 1
Candidate family = A/B/C
ExecuteIndirect
```

変化させるもの:

```text
Pattern P:
  PrefixControl
  UniformStride
  BlockClusteredPermutation
  HashScatterPermutation

Active count |A_t|:
  64, 256, 1024, 2048, 4096

Transition count |T_t|:
  0, 1, 8, 32, 64, 256, 1024, 4096
```

1 Run当たりのCanonical case数は次となる。

```text
4 patterns x 5 Active counts x 8 Transition counts = 160 cases
```

## 4. すべての(A,T)を合法にするCanonical構成

測定点を別の点へsilent clippingすることは禁止する。

### T = 0

```text
A_(t-1) = A_t
M_t = empty
W_t = empty
R_t = empty
H_t = A_t
```

### 0 < T <= A

```text
A_(t-1) = A_t
|M_t| = T
W_t = M_t
R_t = empty
H_t = A_t \ M_t
```

これは`DirtyOnly`である。

### T > A

現在Active Setと交差しないPrevious Active Setを`T-A`個構成する。

```text
A_(t-1) intersect A_t = empty
|A_(t-1)| = T-A
W_t = A_t
R_t = A_(t-1)
H_t = empty
|T_t| = A + (T-A) = T
```

これは`ReplaceAndClear`である。これにより、`A=64, T=4096`のような点も別のcaseへ変形せず正確に測定できる。

## 5. 正しさ確認とTimingの分離

各caseはTiming前に必ず次を通過する。

- Raw Candidateを独立Verifierで検証
- ArtifactとVerified Planの一致
- Write AuditによるAの全4096書込み、B/Cの正確な`T_t`書込み
- Active結果のCPU oracle一致
- Inactive sentinelのbyte一致
- B/Cの`H_t` byte保持
- A/B/C最終出力のbyte一致

Validation shaderはWrite Auditを有効にする。

Timing shaderは出力計算とExecuteIndirect構造を同じままにしてWrite Auditのみ無効化する。これにより、検証専用UAV書込みをPrimary metricへ混入させない。Validation/Timing双方のShader bytecode SHA-256をEvidenceへ固定する。

## 6. Primary metric

```text
GpuCandidateBatchNanosecondsPerEffectiveIteration
```

D3D12 Timestamp Queryで、同一case内のA/B/Cを6種類の全順列で測定する。

```text
ABC, ACB, BAC, BCA, CAB, CBA
```

各Candidateの時間は同じcase、同じattempt、同じorder block内のpaired observationとして比較する。絶対nsは記述値であり、判断はpaired ratioとmedianを中心に行う。

### Timestamp分解能とゼロDispatch

`T=0`ではB/Cの`DispatchGroupsX=0`が正しい。これはShader workが存在しないことを意味し、D3D12 Timestampの開始tickと終了tickが同値になり得る。CU6はこれを失敗や偽の正時間へ変換しない。

```text
expectedDispatchGroupsX = 0 かつ observedTicks = 0
  timestampResolutionCensored = true
  timestampTickCount = 0
  batch time = 1 timestamp tick未満という事実の保守的上限として1 tickを記録
```

この上限値は「実時間が1 tickだった」という主張ではない。実時間がTimestamp分解能より小さいというcensored observationである。Evidence V2は各sampleに`timestampTickCount`、`effectiveIterations`、`timestampResolutionCensored`を記録する。

非ゼロDispatchで0 tickが観測された場合はcensoringを許可しない。同じA/B/C balanced order全体の反復数を2倍にして再測定し、最大65536 iterationsまでに正のintervalを要求する。したがってCandidate間で異なる反復数を使ってpaired orderを壊すことはない。

## 7. Drift拒否

各case blockの前後でCandidate Aを固定Controlとして測定する。

```text
controlRatio = max(before,after) / min(before,after)
```

Defaultでは`controlRatio <= 1.20`のみを受理する。超過時はcase block全体を破棄し、最大3 attemptまで再測定する。Rejected blockのTiming sampleは判断へ使用しない。

## 8. 判断証拠

CU6は各`(Pattern,A,T)`について以下を生成する。

- A/B/C absolute median ns
- Control-normalized median
- noise floor内のObserved winner set
- A対B、B対C、A対Cのpaired ratio判断
- Transition count方向のwinner set変化
- Active count方向のwinner set変化
- 同一(A,T)でのPattern依存

この結果は自動Runtime Policyではない。

```text
RuntimeCandidatePolicyAuthorization = None
UniversalWinnerClaim = Forbidden
```

## 9. 完了の意味

成功したCU6は次を宣言できる。

```text
SGE4-5 SPIRAL 7 EXPERIMENT COMPLETE
```

ただし次は自動ではない。

```text
SPIRAL 7 CLOSED
```

ClosureはOwner判断であり、Closure後の次Programは新しい探索Spiralではなく`Level 4 v2 Canonical reconstruction`である。
