# Level 5 垂直実験1 — Sparse Indirect／Temporal State／UAV Image

## 1. 実験の位置づけ

この実験はLevel 4の新しい一般化ではない。`SGE4UNI 2.7`、Schema 17 Leaf、`SGE4INV 1.5`を変更せず、Generalization 1～7で証明済みの能力を一つの実行問題へ縦に接続する。

目的は、同じ意味と観測を持つ二つの正しい実装候補を作り、Level 5が選択対象とすべき実測差を得ることである。

```text
Candidate A: Dense Direct
  State Writerは最大Universeを毎frame直接Dispatch

Candidate B: Verified Sparse Indirect
  同じState Writer Leaf Packageを使用
  exact transition countだけDispatchIndirect
```

Runtimeや実験Harnessが候補の意味を補正しない。両候補は別のFrozen Composition identityを持ち、どちらも通常のComposition Planner／独立Verifier／Dynamic Planner／独立Verifierを通過する。

## 2. Product境界を変更しない

現行Semantic Compilerの`SampledTexture`はPixel stageへ限定されている。Level 5実験の都合でCompute Texture SRVをProductへ追加することはしない。

したがってRGBA32F Texture branchは、Generalization 5で証明済みのCompute UAV writerとpacked Texture readbackで観測する。State branchはGeneralization 7のTemporal Bufferへ接続する。この構成により、実験の追加だけで`SGE4UNI 2.7`の意味を広げない。

## 3. Canonical payloadとMulti-target route

Canonical member payloadは32 bytesである。

```text
member bytes 0..15   State float4
member bytes 16..31  Color float4
```

Generalization 6のMulti-target routeにより、一つのverified payloadを二つのLeafへ配る。

```text
Canonical payload
  ├─ State slice -> State Writer Dynamic Slot
  └─ Color slice -> Texture Writer Dynamic Slot
```

全routeは同じActive／Update／Clear／Retain集合を共有し、native submit成功後だけHistoryと二つのprivate shadowを原子的にCommitする。

## 4. 垂直pipeline

```text
Verified Dynamic Invocation
  exact Update/Clear/Retain + Canonical payload
        │
        ├─ State Writer
        │    Dense Direct または Verified DispatchIndirect
        │    -> shared State Buffer
        │           ├─ Temporal Producer
        │           │    -> Current State Aggregate
        │           │    -> successful whole-submit後だけPreviousへ回転
        │           └─ Observation Leaf
        │                Previous Aggregate + current State
        │                -> State Observation Buffer
        │
        └─ Texture Writer
             RGBA32F Texture2D UAV
             -> Composition Output
             -> packed Texture readback
```

同時に使うLevel 4能力は次である。

- verified exact Dynamic payload
- multi-target byte-slice routing
- verified DispatchIndirect work count
- RGBA32F Texture2D UAV writer
- packed Texture readback
- depth-one Temporal Buffer
- successful whole-submit後だけのTemporal rotation
- Frozen Composition／Invocation identity
- whole-composition Controlled Recovery

## 5. 比較条件

Universeは`width × height`で固定する。Active memberは常にprefix集合とする。

```text
Active(K) = {0, 1, ..., K-1}
```

この限定はCandidate Bで、

```text
Dispatch X = exact transition count = K
```

とmember indexを一致させるためである。任意の疎index scatterは本実験の対象外であり、結果を一般化してはならない。

各frameでは全Active memberをModified Survivorとして更新する。そのため両候補でCanonical shadowsは同一である。

```text
Dense Direct
  X = Universe
  inactive shadowのzeroもState Bufferへ明示的に書く

Sparse Indirect
  X = K
  prefix KだけをState Bufferへ書く
  inactive領域は初期zeroを保持する
```

State Writerの写像はzero-preservingである。各caseは新しいCompositionから開始し、Kはcase内で固定するため、両候補のState Bufferは同じになる。

Texture Writerは両候補で同じ固定Universe dispatchを行う。これはMulti-target routeとRGBA32F UAV branchの観測を、State Writerの比較対象から分離するためである。

## 6. 観測同値

State Observation Bufferは次のfloat4である。

```text
x = current State Bufferのx成分総和
y = Previous Temporal State Aggregate
z = current - previous
w = Universe
```

successful submit後にPreviousへ昇格したTemporal Aggregateもreadbackする。

```text
x = current State総和
y = 0
z = current State総和
w = Universe
```

さらにRGBA32F Texture Composition Outputをpacked bytesでreadbackし、次を比較する。

- width／height／rowBytes／format
- 全packed bytesのSHA-256
- 各pixelのx成分総和

Dense／Sparse候補でState Observation、Temporal Aggregate、Texture digestが一致しなければ、性能sampleは受理しない。

最初のframeでObservationが読むPreviousは明示的zero seedである。成功Submit後、Temporal ProducerのCurrentだけがPreviousへ回転する。次frameのObservationは、直前frameで受理済みとなったAggregateを読む。

## 7. Timestamp completion境界

Observation Outputだけでは、同じframeのTexture WriterとTemporal Producerの両方が完了したとは限らない。

各frameでは次をすべてreadbackする。

```text
State Observation Buffer
accepted Temporal Aggregate Buffer
RGBA32F Texture Output
```

これにより全4 Leafのcompletionを待ってから`ConsumeTimestampProfileSamples()`を呼ぶ。未完了queryを測定証拠として消費しない。

## 8. 測定境界

D3D12 Executorのexperiment-only timestamp queryを使う。Timestamp profileはFrozen Package bytes、schedule、authority、Resource state、同期を変更しない。

主要測定対象はState Writer Leafの一Dispatchだけである。

```text
記録値
  GPU timestamp nanoseconds
  command recording nanoseconds
```

Texture Writer、Temporal Producer、Observation Leafは意味と完了境界を形成するため実行するが、Dense／Sparseの主要比較値には混ぜない。

各Kについて偶数frameはA→B、奇数frameはB→Aの順で実行し、単純な実行順偏りを抑える。CSVには全raw sample、実行順、GPU値、command-recording値を保存し、同じ行へcase medianとDense／Sparse比も記録する。

標準case:

```text
K = Universe / 64
K = Universe / 16
K = Universe / 4
K = Universe
```

## 9. 判定

実験HarnessはOwnerの最終選択を行わない。

```text
DenseOverSparse > 1.05  -> Sparse優位signal
DenseOverSparse < 0.95  -> Dense優位signal
両方出現                -> Crossover
その他                  -> NoMaterialSeparation
```

これは測定分類であり、Runtime policyではない。出力は常に次で閉じる。

```text
OWNER_DECISION = DeferredByOwner
```

Frozen Variant SetやRuntime candidate selectionは追加しない。

## 10. Recovery資格

測定前に両候補を一frame実行し、Controlled Recoveryを行う。

```text
old Device epoch
  -> whole-composition Controlled Recovery
  -> External rebind acknowledgement
  -> RecoverySeed
  -> Dynamic route shadows再構築
  -> Temporal Previousは明示zero seedへ戻る
  -> State／Temporal／Textureの候補同値を再確認
```

Recovery後も旧Temporal historyや旧Texture handleを再利用しない。

## 11. 実行

```bat
run_sge4_level5_vertical_experiment.bat
```

標準Runnerは次を行う。

1. Source Manifest検証
2. Release build
3. WARP quick corpusでState／Temporal／Texture同値とRecoveryを確認
4. 実GPUで64×64、warmup 6、sample 24を測定
5. CSV evidenceを保存

```text
build/evidence/level5_vertical_v1_warp.csv
build/evidence/level5_vertical_v1_hardware.csv
```

直接実行時の主な引数:

```text
--warp / --hardware
--width N
--height N
--warmup N
--samples N
--output path
--quick
```

## 12. 非目標

- 任意疎indexからIndirect argument／index listを生成すること
- Compute ShaderによるTexture SRV readをProductへ追加すること
- GPU generated count buffer
- multiple Indirect target
- routeごとの独立membership
- Frozen Variant Set
- Runtime performance policy
- TextureのTemporal化
- Partial Recovery
- 実験結果から自動的にProduction候補を採用すること

この実験の価値は「Sparseが常に速い」と主張することではない。Level 4で証明された複数能力を一つの実行問題へ接続し、同じ観測意味を持つ候補間で、どの入力領域に実測差が生じるかをLevel 5の証拠として固定することである。
