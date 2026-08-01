# Level 4 Generalization 7 — Verified Temporal Buffer Flow

## 1. 目的

Generalization 1～6までのComposition Resource Flowは、すべて同一frame内でproducerからconsumerへ渡されていた。Runtime Sessionが保持するDynamic Historyとprivate shadowsはmembership／payload受理履歴であり、前frameにGPUが生成したResourceを次frameのLeafへ渡すResource historyではない。

Generalization 7は、固定size Bufferについて、前回受理済みframeのGPU結果を次frameの入力として扱う最小Temporal Flowを正本化する。

```text
Frame N
  Previous[N-1] -> Reader Leaf
  Writer Leaf   -> Current[N]
  全Leaf Submit成功
          ↓ atomic temporal commit
  Previous[N] = Current[N]

Frame N+1
  Previous[N] -> Reader Leaf
```

Runtimeは「直前に書かれたように見えるBuffer」を推測しない。Composition Contract／PlanがTemporal lifetime、history depth、Current writer、Previous readers、物理instance数を固定する。

## 2. 限定範囲

初期能力は次だけを扱う。

```text
Resource kind          Buffer
Extent                 fixed sizeBytes
Lifetime               TemporalHistory
History depth          1
Physical generations   2 (Previous / Current)
Writer                 exactly one
Readers                one以上
Writer / readers       unconditional Leaf
Adapter                 single adapter / shared device domain
Recovery unit           whole Composition
Initial state           explicit full-size Previous seed
```

Temporal Texture、history depth 2以上、ring buffer、partial range history、Conditional writer／reader、multiple writer、cross-adapter、partial recoveryは含めない。

## 3. Frozen Composition ABI

Production Frozen Compositionは`SGE4UNI 2.7`である。

```text
SGE4UNI 2.7
  Manifest schema 2
  Leaf Table schema 1
  complete Schema 17 Leaf Package bytes
  Contract Data schema 3
  Verified Decision Data schema 3
  Verification Certificate schema 1
  Authority Ledger schema 2
  Dynamic Contract schema 5
```

Leaf PackageはSchema 17、Frozen Dynamic Invocationは`SGE4INV 1.5`を維持する。

Contract Data schema 3はResource Flowへ次を追加する。

```text
lifetime
  SameFrame
  TemporalHistory
historyDepth
```

Verified Decision Data schema 3はallocationへ同じlifetime／historyDepthと`physicalInstanceCount`を保存し、専用`TemporalBufferPlan`を所有する。

```text
TemporalBufferPlan
  resource
  currentProducer
  currentProducerLeaf
  historyDepth = 1
  physicalInstanceCount = 2
  previousConsumers[]
```

## 4. Same-frame DAGとの分離

Temporal Bufferは同一frameのproducer→consumer依存ではない。

```text
Current writer[N]  ─X→ Previous reader[N]
Current writer[N]   →  Previous reader[N+1]
```

したがってTemporal resourceは、通常のComposition handoff、signal、wait、同一frame DAG edgeへ入れない。PlannerはTemporal edgeを除いたsame-frame graphからscheduleを導出し、Temporal関係は専用Planへだけ固定する。

独立VerifierもContractから同じ分離を再導出し、Temporal resourceが通常handoff／signal／waitへ混入した成果物を拒否する。

## 5. Runtimeの二世代物理モデル

Composition-owned Temporal Bufferごとに、同じsizeを持つ二つの独立native Bufferを物質化する。

```text
record.instances[Previous]
record.instances[Current]
```

Endpoint bindingはFrozen accessから機械的に決まる。

```text
ReadOnly endpoint  -> Previous instance
WriteOnly endpoint -> Current instance
```

各instanceは独立したResource handle、state、completion tokenを所有する。Previous readerはCurrent writerの同一frame completionを待たない。

## 6. Atomic Temporal Commit

全enabled Leafのnative submissionが成功し、Temporal Current writerが実行された後だけ、Previous／Current indexを交換する。

```text
成功:
  submit all leaves
  -> swap Previous / Current
  -> 新しい受理済みhistory

途中失敗:
  -> swapしない
  -> accepted Previousを維持
```

RuntimeはCurrent内容をCPUへcopyしてPreviousを作らない。二つの物理Bufferの役割indexだけを交換する。

Initial GeneralizationではTemporal writer／readerをunconditionalに限定するため、成功frameでwriter不在となるpolicy ambiguityを作らない。

## 7. Initial seed

Temporal BufferはLoad時に固定sizeと完全一致する明示的seedを要求する。

```text
seed bytes == resource.sizeBytes
```

PreviousとCurrentの両物理instanceをseedで初期化する。最初のframeのreaderはseedを読む。暗黙zero、直前の任意外部Buffer、CPU callbackからの推測は行わない。

## 8. Recovery

Whole-composition RecoveryではTemporal二世代Resource、state、completion、role indexをすべて失効させる。

External rebind acknowledgement後、Load時に保持した明示的seedからPrevious／Currentを再物質化する。

```text
Controlled Recovery
  -> old Previous / Current handle失効
  -> history invalid
  -> explicit seedで二世代再作成
  -> Previous = seed
  -> 次のRecovery frameを実行
```

失われたGPU historyをRuntimeが推測または復元しない。

## 9. 資格試験

Architecture Gate:

- `SGE4UNI 2.7`／Contract Data schema 3／Decision Data schema 3
- TemporalHistory／historyDepth 1のRound-trip
- physicalInstanceCount 2
- Current writer／Previous readerを専用Temporal Planへ固定
- Temporal resourceがhandoff／signal／waitへ入らないこと
- history depth 0拒否
- Temporal Texture拒否
- Conditional writer／reader拒否
- ABI 1.1 migrationがTemporal Flowを推測しないこと
- Generalization 1～6回帰

Windows Gate:

```text
seed = float4(10)

Frame 0
  reader sees Previous seed 10
  consumer output = 11
  writer writes Current = 20
  successful commit
  observable Previous = 20

Frame 1
  reader sees Previous 20
  consumer output = 21

Controlled Recovery
  Temporal history失効
  seedから再物質化
  recovery frame output = 11
```

これにより、same-frame writer結果をreaderが誤って読む実装、失敗前のCurrentを受理する実装、Recovery後に旧historyを残す実装を区別する。

## 10. 意図的な非範囲

- Temporal Texture2D
- history depth 2以上
- arbitrary ring size
- multiple Current writers
- Conditional Temporal writer／reader
- writer未選択時のHold／Rotate／Invalidate policy
- partial Buffer range history
- temporal aliasing
- GPU-generated generation index
- Streaming／Residencyとの合成
- partial temporal recovery
- cross-adapter history

Generalization 7の目的はTemporal機能数を増やすことではなく、「前回受理済みGPU Resource」という時間をまたぐ事実を、Frozen Contract、Verified Plan、native resource generations、Recoveryまで権威切れなく接続することである。
