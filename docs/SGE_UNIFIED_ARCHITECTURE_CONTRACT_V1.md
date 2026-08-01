# SGE Unified Architecture Contract V1

## 1. Reconstruction purpose

New SGE4は機能追加ではない。Level 4 v2までに成立した意味、権威、実行、観測、Recoveryの事実を、一つの実行可能なArchitectureへ再具現化する。

## 2. Two compiler stages

### Leaf stage

Leaf stageは一つの閉じたGPU実行単位を所有する。

- Semantic Graph
- ResourceUse／dependency
- queue／state／allocation／binding
- operation stream
- independent verification
- complete Frozen Leaf Package
- Package factsから決定されるLeaf Certificate

Leaf PlannerはVerifierを呼ばない。VerifierだけがPlanを受理できる。

### Composition stage

Composition stageはFrozen Leaf内部を再解釈しない。

- Leaf Interface
- Endpoint／Flow
- shared resource
- inter-Leaf schedule
- handoff／synchronization
- presenter
- whole-composition recovery set
- independent verification
- complete Frozen Composition Package
- complete Compositionから決定されるComposition Certificate

Composition PlannerはVerifierを呼ばない。VerifierだけがCompositionをSealできる。

## 3. Common canonical core

共通化するものはIdentity、Digest、Version、Canonical encoding、Target compatibility、Device epoch、Errorである。Leaf GraphとComposition Graph、各Planner、各Verifierは統合しない。

Certificateは新たな判断主体ではない。検証済み完全Artifactから機械的に導出されるidentity束である。

## 4. Frozen hierarchy

```text
SGE4UNI Frozen Composition Package 2.4
  Manifest schema 2
  Leaf Table schema 1
  Embedded complete Schema 17 Leaf Package bytes
  Contract Data schema 2
  Verified Decision Data schema 2
  Verification Certificate schema 1
  Authority Ledger schema 2
  Dynamic Contract schema 5
```

内側`SGE4CMP` ContainerはProduction ABIから廃止する。Compositionの全実行事実は`SGE4UNI 2.6`が直接所有し、Leaf Packageだけを独立した下位Frozen Artifactとして保持する。

同一bytesは同一意味を持つ。Compilerなしで読込・検証・実行・Recovery再物質化ができなければならない。Production ReaderはABI 2.6だけを受理し、旧ABI Readerはmigration treeへ隔離する。

## 5. Dynamic execution

Dynamic InvocationはComposition構造を変更しない。Active membership、Modified survivor、History、Device epochを明示入力とし、activation、deactivation、update、retain、transition、write set、indirect quantityをPlannerと独立Verifierが再導出する。

Composition Dynamic Contract schema 5は`AuthorityOnly`または`VerifiedDenseSlot`を固定する。VerifiedDenseSlotではCanonical member byte幅と一つ以上のtarget Leaf／Dynamic Slot／source slice routeをCompositionが所有し、SGE4INV 1.5が同じroute table、exact Canonical Update payload、payload identityを所有する。Update payloadのmember集合はexact Update setと完全一致しなければならない。

Runtimeへ渡せるのは`FrozenDynamicInvocationPackage`だけである。ContinueHistory成果物は、Verifierが使用した前History identityをFrozen成果物へ保存し、Runtimeが現在受理しているHistory identityと一致しなければならない。Raw requestをRuntime内部でPlan／Verifyすることは禁止する。Runtimeはverified Update／Clearだけをprivate dense shadowへ適用し、native submission成功後にHistoryとshadowを同時Commitする。

## 6. Runtime

RuntimeはPlannerではない。Frozen Composition、Frozen Dynamic Invocation、およびAuthorityOnly routeに対する明示的Leaf dynamic bytesだけを消費する。VerifiedDenseSlotのbytesはFrozen Invocationから機械的に構築し、Caller上書きを拒否する。性能測定から候補を選ばない。

Portable `Runtime Session`がepoch、handle、history、external rebind、RecoverySeed authorityを所有する。D3D12 Runtimeはこれをnative object lifetimeへ写像する。

## 7. Recovery

Composition全体をRecovery unitとする。Recoveryは全Runtime object、旧epoch Handle、Temporal Historyを失効させる。再開にはExternal Rebind acknowledgementとRecoverySeedが必要である。

## 8. Backend

`13_LeafArtifact`がCompiler／Executor共通のSchema 17 frozen schema／encodingを所有する。D3D12 CompilerはTarget profileとVerified PlanからそのSchemaへのLowering、およびHLSL compileを所有する。D3D12 Executorは既に固定されたLeaf operationとComposition scheduleをAPI呼出しへ機械的に写像する。Queue、state、allocation、present、recovery scope、dynamic membershipを再判断しない。

## Generalization 2 amendment

Conditional Regionの意味は現行SGE4UNI 2.6 Dynamic Contract schema 5へ保持する。predicateとTrue／False Leaf集合はComposition authorityが所有し、Dynamic Plannerと独立Verifierはexact membership algebraからRegion selectionとenabled Leaf集合を別々に導出してSGE4INV 1.5へSealする。Runtimeはpredicateを再評価せず、Seal済みenabled LeafだけをFrozen schedule順でSubmitする。


## Generalization 3 amendment

Production Frozen CompositionはSGE4UNI 2.6、Contract Data schema 2、Verified Decision Data schema 2とする。Composition Modelは埋込みSchema 17 Leaf endpointからfixed BGRA8 Texture2D shapeを再導出し、Plannerと独立Verifierはsingle writer、same-frame DAG、exact extent／formatを検証する。Runtimeはshared TextureをRTV producerからSRV consumerへstate／completion付きで渡し、ExecutorだけがD3D12 row pitchを処理する。


## Generalization 4 amendment

Production Frozen CompositionはSGE4UNI 2.6、Dynamic Contract schema 5とする。Composition authorityは一つのunconditional Compute Leaf／Compute CommandとmaxWorkCountを固定する。Dynamic Plannerと独立Verifierはexact Transition setからworkCountとDispatch X／Y／Zを別々に導出し、SGE4INV 1.5へSealする。

RuntimeはworkCountを再計算せず、Seal済みroute／identity／上限だけを検査してExecutorへ渡す。D3D12 Executorは対象`ExecuteCompute`だけを`ExecuteIndirect(DISPATCH)`へ置換し、対象外Commandの固定Dispatchを維持する。

## Limited Texture2D UAV／Compute Flow

Production SGE4UNI 2.6は、fixed R32G32B32A32_FLOAT Texture2Dを一つのCompute UAV writerからSRV consumerへ接続できる。Semantic CompilerはStorageTexture2D／UnorderedTexture2Dを検証し、Schema 17 PackageがUAV viewとUnorderedWrite stateを所有する。Composition Planner／VerifierはRGBA32F shape、single writer、UnorderedWrite→ShaderRead handoffを固定する。RuntimeとExecutorはnative usageを再選択せず、UAV-capable shared Textureを物質化して同じresourceをconsumerへ渡す。

## Multi-target verified routing

一つのDynamic universeに対するexact membership algebraは全routeで共通である。CompositionはCanonical member payloadの固定sliceを複数target Slotへ写像する。Plannerと独立Verifierはroute table、slice範囲、target Slot容量、payload集合とidentityを別々に検証する。Runtimeは全route shadowを候補状態へ更新し、native submit成功後にHistoryと原子的にCommitする。
