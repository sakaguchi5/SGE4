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
SGE4UNI Frozen Composition Package 2.2
  Manifest schema 2
  Leaf Table schema 1
  Embedded complete Schema 17 Leaf Package bytes
  Contract Data schema 1
  Verified Decision Data schema 1
  Verification Certificate schema 1
  Authority Ledger schema 2
  Dynamic Contract schema 3
```

内側`SGE4CMP` ContainerはProduction ABIから廃止する。Compositionの全実行事実は`SGE4UNI 2.2`が直接所有し、Leaf Packageだけを独立した下位Frozen Artifactとして保持する。

同一bytesは同一意味を持つ。Compilerなしで読込・検証・実行・Recovery再物質化ができなければならない。Production ReaderはABI 2.2だけを受理し、旧ABI Readerはmigration treeへ隔離する。

## 5. Dynamic execution

Dynamic InvocationはComposition構造を変更しない。Active membership、Modified survivor、History、Device epochを明示入力とし、activation、deactivation、update、retain、transition、write set、indirect quantityをPlannerと独立Verifierが再導出する。

Composition Dynamic Contract schema 3は`AuthorityOnly`または`VerifiedDenseSlot`を固定する。VerifiedDenseSlotでは対象Leaf、Dynamic Slot、member byte幅をCompositionが所有し、SGE4INV 1.3がexact Update payloadとpayload identityを所有する。Update payloadのmember集合はexact Update setと完全一致しなければならない。

Runtimeへ渡せるのは`FrozenDynamicInvocationPackage`だけである。ContinueHistory成果物は、Verifierが使用した前History identityをFrozen成果物へ保存し、Runtimeが現在受理しているHistory identityと一致しなければならない。Raw requestをRuntime内部でPlan／Verifyすることは禁止する。Runtimeはverified Update／Clearだけをprivate dense shadowへ適用し、native submission成功後にHistoryとshadowを同時Commitする。

## 6. Runtime

RuntimeはPlannerではない。Frozen Composition、Frozen Dynamic Invocation、およびAuthorityOnly routeに対する明示的Leaf dynamic bytesだけを消費する。VerifiedDenseSlotのbytesはFrozen Invocationから機械的に構築し、Caller上書きを拒否する。性能測定から候補を選ばない。

Portable `Runtime Session`がepoch、handle、history、external rebind、RecoverySeed authorityを所有する。D3D12 Runtimeはこれをnative object lifetimeへ写像する。

## 7. Recovery

Composition全体をRecovery unitとする。Recoveryは全Runtime object、旧epoch Handle、Temporal Historyを失効させる。再開にはExternal Rebind acknowledgementとRecoverySeedが必要である。

## 8. Backend

`13_LeafArtifact`がCompiler／Executor共通のSchema 17 frozen schema／encodingを所有する。D3D12 CompilerはTarget profileとVerified PlanからそのSchemaへのLowering、およびHLSL compileを所有する。D3D12 Executorは既に固定されたLeaf operationとComposition scheduleをAPI呼出しへ機械的に写像する。Queue、state、allocation、present、recovery scope、dynamic membershipを再判断しない。

## Generalization 2 amendment

Production Frozen CompositionはSGE4UNI 2.2、Dynamic Contract schema 3とする。Conditional RegionのpredicateとTrue／False Leaf集合はComposition authorityが所有する。Dynamic Plannerと独立Verifierはexact membership algebraからRegion selectionとenabled Leaf集合を別々に導出し、SGE4INV 1.3へSealする。Runtimeはpredicateを再評価せず、Seal済みenabled LeafだけをFrozen schedule順でSubmitする。
