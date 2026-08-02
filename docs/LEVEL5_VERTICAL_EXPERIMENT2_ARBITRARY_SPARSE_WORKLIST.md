# Level 5 垂直実験2 — Arbitrary Sparse Worklist

## 1. 実験の位置づけ

この実験はLevel 4 Generalization 8で導入した`Verified Compact Sparse Worklist`を、Level 5の比較候補として使用する。

Level 5垂直実験1ではActive集合をprefixへ限定した。

```text
Active(K) = {0, 1, ..., K-1}
```

垂直実験2では同じKについて複数のCanonical任意疎集合を構築し、次を同時に確かめる。

1. Generalization 8が非prefix集合を正しいGPU memberへ接続すること
2. Dense DirectとVerified Compact Sparse Worklistの観測意味が一致すること
3. work数Kが同じでもmember分布とmemory localityによってGPU時間が変わるか
4. Temporal history、Texture UAV観測、Controlled Recoveryが任意疎集合でも維持されること

Product ABI、Runtime policy、Frozen Variant Setは変更しない。

## 2. 比較候補

両候補は同じ4個のSchema 17 Leaf Package、同じComposition Contract、同じResource graphを共有する。

```text
Candidate A: Dense Direct + identity index list
  Dispatch X = Universe
  CompactIndices[ordinal] = ordinal
  State WriterはUniverse全体を実行

Candidate B: Verified Compact Sparse Worklist
  DispatchIndirect X = exact Transition set count
  CompactIndices[ordinal] = verified member ID
  State Writerはverified worklistに含まれるmemberだけを実行
```

State Writer Leaf自体は両候補で同一である。

```hlsl
member = CompactIndices[SV_DispatchThreadID.x]
value  = DynamicState[member]
StateOutput[member] = Transform(value)
```

候補差分はFrozen Dynamic ContractのIndirect／Compact Worklist authorityだけである。

## 3. 垂直pipeline

```text
Verified Dynamic Invocation
  exact Active／Modified／Transition set
  Canonical 32-byte member payload
       │
       ├─ State slice
       │    → State Writer Dynamic Slot 0
       │    → Candidate A: caller-owned identity index Slot 1
       │    → Candidate B: verified Compact Worklist Slot 1
       │    → State Buffer
       │         ├─ Temporal Producer
       │         │    → Current Aggregate
       │         │    → successful whole-submit後だけPreviousへ回転
       │         └─ Observation Leaf
       │              → current／Previous／delta
       │
       └─ Color slice
            → Texture Writer
            → RGBA32F Texture2D UAV
            → packed Texture readback
```

同時に使用する証明済み能力:

- exact Dynamic algebra
- Multi-target byte-slice routing
- Verified DispatchIndirect
- Verified Compact Sparse Worklist
- RGBA32F Texture2D UAV
- packed Texture readback
- Temporal Buffer depth 1
- successful whole-submit後のTemporal rotation
- whole-composition Controlled Recovery

## 4. Workset分布

各Worksetは昇順、一意、Universe内のCanonical uint32 member ID列として生成する。

### Prefix

```text
{0, 1, ..., K-1}
```

垂直実験1との接続点である。

### Suffix

```text
{Universe-K, ..., Universe-1}
```

同じ連続長KをBuffer末尾へ移動する。

### UniformStride

```text
member[i] = floor(i * Universe / K)
```

Universe全域へほぼ等間隔に分散する。

### Clustered4

Universeを最大4領域へ分け、各領域中央に連続clusterを配置する。局所連続性を持つ複数clusterを表す。

### SeededRandom

固定seed方式`L5VERT21-xorshift64star-universe-active`で一意indexを選び、Canonical昇順へ整列する。同じUniverseとKでは必ず同じWorksetになる。

標準K:

```text
Universe / 64
Universe / 16
Universe / 4
```

100% activeは垂直実験1でDense／Sparseが同一中央値へ収束済みであり、本実験では分布差が消えるため除外する。

## 5. Locality evidence

各Worksetについて次をCSVへ保存する。

```text
workset SHA-256
span = max(member) - min(member) + 1
contiguous run数
平均gap
最大gap
```

これはGPU時間との相関を後から再評価するための記述量であり、Runtimeが使用するperformance policyではない。

## 6. Dynamic authority

Candidate Bでは、Dynamic Plannerと独立Verifierがexact Transition setから同じCanonical worklistを再導出する。

```text
Active／Modified／Deactivated
       ↓
exact Transition set
       ↓
Canonical ascending uint32 list
       ↓
Compact Worklist identity
       ↓
SGE4INV 1.6 Section kind 8
       ↓
fixed-size Dynamic Slot 1
       ↓
DispatchIndirect X = list count
```

実験HarnessがSparse candidateのindex listを生成、並べ替え、補正することはない。

Candidate Aだけは比較基準として、Composition authorityが所有していないSlot 1へ固定identity list`[0,1,...,Universe-1]`をFrameInputから渡す。このlistは毎frame同一であり、Active集合の選択には関与しない。

## 7. 観測同値

各frameで次をDense／Sparse間比較する。

```text
State Observation float4
accepted Temporal Aggregate float4
RGBA32F Texture packed bytes SHA-256
Texture x成分総和
```

State Observation:

```text
x = current State Buffer総和
y = Previous Temporal Aggregate
z = current - previous
w = Universe
```

候補同値が成立しないsampleは性能証拠として受理しない。

## 8. 測定

主要対象はState Writer Leafの一Dispatchだけである。

```text
GPU timestamp nanoseconds
command recording nanoseconds
```

各Workset caseは独立Composition Loadから開始する。caseごとにwarmupを行い、その後raw sampleを記録する。

実行順は交互にする。

```text
偶数frame A → B
奇数frame B → A
```

## 9. 分類

全caseでSparseが5%以上優位であり、同じKのSparse中央値の最大／最小が10%を超える場合:

```text
ArbitrarySparseAdvantageDistributionSensitive
```

全caseでSparseが5%以上優位だが分布spreadが10%以下の場合:

```text
ArbitrarySparseAdvantageDistributionStable
```

Dense優位caseが一つでも存在する場合:

```text
MixedCandidateAdvantage
```

それ以外:

```text
NoMaterialSeparation
```

最終選択は常に次で閉じる。

```text
OWNER_DECISION = DeferredByOwner
```

## 10. Recovery資格

固定seed乱択Worksetを使い、両候補で次を確認する。

```text
任意疎InitialSeed
  → State／Temporal／Texture同値
  → Controlled Recovery
  → Device epoch更新
  → External rebind acknowledgement
  → 同じ任意疎RecoverySeed
  → Previous Temporalは明示zero seedへ戻る
  → State／Temporal／Texture同値
```

旧Device epochのCompact Worklist bindingやTemporal historyを再利用しない。

## 11. Evidence

```text
build/evidence/level5_vertical_v2_warp.csv
build/evidence/level5_vertical_v2_hardware.csv
```

主な列:

```text
distribution
workset_digest
active_count／active_ratio
span／contiguous_runs／mean_gap／max_gap
sample_index／execution_order
dense_gpu_ns／sparse_gpu_ns
dense_recording_ns／sparse_recording_ns
case median／DenseOverSparse
```

## 12. 実行

```bat
run_sge4_level5_arbitrary_sparse_worklist_experiment.bat
```

RunnerはSource Manifest検証、Release build、WARP quick資格、実GPU 64×64測定を行う。

## 13. 非目標

- GPU生成worklist
- unsortedまたは重複indexの受理
- routeごとの独立membership
- multiple Indirect target
- RuntimeによるDense／Sparse自動選択
- Frozen Variant Set
- Active集合がframeごとに異なるchurn比較
- Temporal差分aggregateそのものの性能比較

この実験は、Generalization 8の任意疎実行能力をLevel 5の同値候補比較へ接続し、Kと分布の二軸を初めて分離して測定する。

## 13. 正式結果

Windows／WARP／実GPUで取得した正式結果は次へ固定した。

```text
docs/LEVEL5_VERTICAL_EXPERIMENT2_RESULT.md
```

実GPUではUniverse 4096、5分布、3密度の全15 case、全360 paired sampleでSparseが勝利し、分類は`ArbitrarySparseAdvantageDistributionStable`となった。Active率25%～100%およびUniverse依存の交差位置は、Level 5垂直実験2bで測定する。
