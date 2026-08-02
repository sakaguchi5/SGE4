# New SGE4 — Unified Two-Stage Compiler Reconstruction

> Revision 2.8: Level 4 Generalization 8として`Verified Compact Sparse Worklist`を導入した。exact Transition setをCanonical昇順のuint32 member ID列へ独立再導出し、`SGE4INV 1.6`の必須Compact Worklist SectionへSealする。Runtimeは固定長Dynamic Slotへzero padding付きで物質化し、Dispatch ordinalを任意のverified member IDへ接続する。
>
> Level 5 Vertical Experiment 1 Result: `SGE4UNI 2.7`上のDense DirectとVerified Sparse Indirectについて、State／Temporal／Texture／Recoveryの観測同値を確認した。実GPUではActive率1.5625%、6.25%、25%でSparseが全paired sampleに勝ち、100%では同一中央値へ収束した。正式結果は`docs/LEVEL5_VERTICAL_EXPERIMENT1_RESULT.md`へ固定した。
>
> Revision 2.7: Level 4 Generalization 7として`Verified Temporal Buffer Flow`を導入した。固定size Buffer、history depth 1、Previous／Current二世代、single unconditional writerをComposition ContractとPlanへ固定し、全LeafのSubmit成功後だけ世代を原子的に回転する。明示的seed、packed readback、whole-composition Recoveryを資格化した。
>
> Revision 2.6: Level 4 Generalization 6として`Multi-target Verified Dynamic Routing`を導入した。一つのCanonical member payloadをComposition固定のbyte sliceで複数Leaf／複数Dynamic Slotへ配布し、`SGE4INV 1.5`がCanonical payloadとroute tableをSealする。Runtimeは全routeのUpdate／Clearをprivate shadowsへ一括適用し、native submit成功後にHistoryと全shadowを原子的にCommitする。
>
> Revision 2.5: Level 4 Generalization 5として限定`Texture2D UAV／Compute Flow`を導入した。固定RGBA32F、single mip／layer／plane／sampleのCompute UAV writerをComposition ContractとPlanへ固定し、shared TextureをUnorderedWriteからSRV consumerへstate／completion付きで接続する。中間RGBA32Fと最終BGRA8のpacked readback、whole-composition Recoveryを資格化した。
>
> Revision 2.4: Level 4 Generalization 4として`Verified Indirect Work Execution`を導入した。Compositionが一つのunconditional Compute Leaf／Compute Commandと最大work数を固定し、`SGE4INV 1.4`がexact transition countから導いたDispatch引数をSealする。Runtimeは件数を再計算せず、Executorが対象CommandだけをD3D12 `ExecuteIndirect(DISPATCH)`へ機械的に置換する。
>
> Revision 2.3: Level 4 Generalization 3として限定`Texture2D Flow`を導入した。固定BGRA8、single mip／layer／plane／sample、single writer、same-frameのTexture形状をComposition ContractとPlanへ固定し、shared TextureをRTV producerからSRV consumerへ状態・completion付きで接続する。ExecutorはD3D12 row pitchを機械的に処理し、packed readbackを返す。
>
> Revision 2.2: Level 4 Generalization 2として非ネスト型`Conditional Region`を導入した。CompositionがpredicateとTrue／False Leaf集合を固定し、Dynamic Plannerと独立Verifierがexact setからbranch選択とenabled Leaf集合をSealする。Runtimeはpredicateを再評価せず、SGE4INV 1.3の選択結果どおりに未選択LeafをSubmitしない。
>
> Revision 2.1: Level 4 Generalization 1として`Verified Dynamic Execution`を導入した。Compositionが一つのLeaf Dynamic Slotへのdense member routeを固定し、`SGE4INV 1.2`がexact Update payloadをSealする。Runtimeはverified Update／Clearだけをprivate shadowへ適用し、GPU submit成功後にHistoryとshadowを同時Commitする。Callerによる対象Slotの上書きは禁止する。
>
> Revision 2.0: `NewSGE4 v1.5.2 FULL GATE PASSED`を変更不能Oracleとして、Frozen Compositionを平坦な`SGE4UNI 2.0`へ移行した。内側の`SGE4CMP 1.0` ContainerはProduction Artifactから廃止し、Leaf Table、Schema 17 Leaf bytes、Contract、Verified Decision、Verification Certificate、Authority Ledger、Dynamic Contractを外側Containerが直接所有する。Leaf Schema 17と`SGE4INV 1.1`は維持する。
>
> Revision 1.5: `v1.4 FULL GATE PASSED`を変更不能Baselineとして、独自Resultを`std::expected`へ移行し、Canonical encodingとSchema検証をC++23で型安全化した。巨大なD3D12実装を責務別Sourceへ分割し、人間向けDiagnosticとQualification表示を日本語化した。Frozen ABI 1.xは変更していない。
>
> Revision 1.4: `v1.3.1 FULL GATE PASSED`を変更不能Baselineとして、`src/internal/`に残っていた旧Level／R別実装を、最終Architectureの責務へ再構築した。実行可能なLeaf／Composition PlanとArtifactを正本とし、v2のidentity・seal・拒否条件をそこへ吸収した。RuntimeからDynamic Planner／Verifierを排除し、D3D12 CompilerとExecutorを正式に分離した。
>
> Revision 1.3.1: 41製品プロジェクトを14責務プロジェクトへ統合し、Windows Full Gateを通過。Revision 1.2: MSBuild node reuseを無効化。Revision 1.1: R4回帰試験の誤った期待値を修正。

この成果物は、`SGE4_Level4v2.zip`までに証明された範囲だけを、次の一つの実行Architectureへ再具現化したものです。

```text
Semantic Graph
  -> Leaf Planner
  -> Independent Leaf Verifier
  -> complete Frozen Leaf Package + Leaf Certificate
  -> Composition Planner
  -> Independent Composition Verifier
  -> complete Frozen Composition Package + Composition Certificate
  -> Dynamic Planner
  -> Independent Dynamic Verifier
  -> Frozen Dynamic Invocation Package
  -> Runtime Session / whole-Composition Recovery
  -> D3D12 Executor
```


## Revision 1.5 C++23 Source Reconstruction

- `/std:c++latest`を維持
- 独自`Result<T,E>`を廃止し、`std::expected<T,E>`へ統一
- `std::to_underlying`、件数の安全な縮小変換、`consteval` Schema検証を追加
- Canonical sort／検索に限定してRangesを使用
- D3D12 Encoding／Lowering／Executorの巨大Sourceを責務別`.inl`へ分割
- SourceとログをUTF-8とし、人間向けメッセージを日本語化
- Revision 1.5ではSchema 17、SGE4CMP 1.0、SGE4UNI 1.1、SGE4INV 1.1を維持
- v1.4とのPortable Golden bytes一致試験をArchitecture Gateへ追加
- Revision 2.0ではCompositionだけを`SGE4UNI 2.0`へ移行し、Schema 17とSGE4INV 1.1は維持

Modules、`flat_map`／`flat_set`への置換、`mdspan`、`print`、`unreachable`、`assume`、複雑なViews pipelineは導入していません。

詳細は`docs/CPP23_SOURCE_RECONSTRUCTION_V1.md`を参照してください。

## Revision 1.4で解消した過渡構造

### 1. `src/internal/`を廃止

Active sourceはArchitectureの責務へ直接配置されています。

```text
src/
  canonical/
  leaf/
  composition/
  dynamic/
  runtime/
  backends/d3d12/
```

旧Level番号・R番号を持つSource pathはActive treeから消えました。吸収済みのv2 identity-only実装は、削除せず`reference/retired_source/`へ退役しています。

### 2. Leaf authorityを完全Artifactへ統合

Leaf Compilerは、独立Verifierを通過した完全なSchema 17 Frozen Leaf Packageを生成します。`LeafCertificate`はその実PackageとD3D12 Package Viewから直接決定されます。別のv2 Planner／Verifierを後段で再実行する二重authority経路はありません。

### 3. Composition authorityを完全Planへ統合

Compositionは、ContractからPlanを一度だけ提案し、独立VerifierでSealし、平坦な`SGE4UNI 2.8`へFreezeします。`CompositionCertificate`は、ABI 2.8 Composition Core、検証済みContract、Plan、Seal、Schedule、Recovery Setから直接決定されます。identityだけの第二Composition経路はありません。

### 4. RuntimeからPlanner／Verifierを排除

RuntimeはRaw Dynamic Invocationを受けません。

```text
Invocation input
  -> Dynamic Planner
  -> Independent Dynamic Verifier
  -> Frozen Dynamic Invocation
  -> Runtime Submit
```

RuntimeはFrozen CompositionとFrozen Dynamic Invocationを検証して消費するだけです。性能測定や実行時観測からPlanを作り直しません。

### 5. D3D12 Compiler／Executorを分離

```text
13_LeafArtifact
  Schema 17 Package core
  D3D12 frozen schema / encoding
  Leaf certificate

50_D3D12Compiler
  target profile
  verified-plan lowering
  HLSL compile
  Leaf compile orchestration

51_D3D12Executor
  device domain
  materialization
  submission
  shared resources
  readback
  recovery
```

PortableなCanonical／Composition／Dynamic／Runtime CoreにはWindows／D3D12の実行詳細を所有させません。Schema 17のD3D12 frozen schema／encodingは、CompilerとExecutorの双方が読む共有Artifact契約として`13_LeafArtifact`が保持します。

## Active project structure

### Product — 15 projects

```text
01_CanonicalCore

10_LeafModel
11_LeafVerifier
12_LeafPlanner
13_LeafArtifact

20_CompositionModel
21_CompositionPlanner
22_CompositionVerifier
23_CompositionArtifactToolchain

30_DynamicModelArtifact
31_DynamicPlanner
32_DynamicVerifier

40_RuntimeCore

50_D3D12Compiler
51_D3D12Executor
```

### Qualification — 3 projects

```text
60_UnifiedArchitectureTests
61_UnifiedWindowsQualification
62_UnifiedMigrationAcceptance
```

PlannerとVerifierは、Leaf／Composition／Dynamicの各段で別プロジェクトのままです。共通化するのは型、Canonical encoding、Digestなど、判定結果を作らない基盤だけです。

## Frozen artifact hierarchy

```text
SGE4UNI Frozen Composition Package 2.8
  Manifest schema 2
  Leaf Table schema 1
  complete Schema 17 Leaf Package bytes
  Contract Data schema 3
  Verified Decision Data schema 3
  Verification Certificate schema 1
  Authority Ledger schema 2
  Dynamic Contract schema 6
```

`CompleteComposition` Sectionと内側`SGE4CMP 1.0`はProduction ABIから廃止しました。Leaf Packageの独立ABIはSchema 17のまま維持し、Leaf bytesを再符号化せず完全に埋め込みます。

Production Readerは`SGE4UNI 2.8`だけを受理します。`SGE4UNI 1.1`／`SGE4CMP 1.0` Reader／Writerは`src/composition/migration/abi1/`へ隔離され、明示的な資格試験用Migration Toolだけが使用します。

Dynamic Invocationは別の`SGE4INV` major 1／minor 6、Manifest schema 7成果物です。Conditional Execution SectionがRegion選択とenabled Leaf集合を、Execution Payload schema 2がCanonical member payload、複数route、exact Update payloadを、Indirect Dispatch Sectionが対象Compute routeとSeal済みDispatch引数を、Compact Worklist Sectionがexact Transition setから独立導出されたCanonical uint32 member ID列を保存します。Runtimeは、ABI 2.8 Composition identity、受理済みHistory identity、worklist identityが一致する成果物だけをSubmitできます。

詳細は次を参照してください。

```text
docs/FROZEN_COMPOSITION_ABI_2_0.md
docs/FROZEN_COMPOSITION_ABI_2_0_MIGRATION.md
docs/LEVEL4_GENERALIZATION1_VERIFIED_DYNAMIC_EXECUTION.md
docs/LEVEL4_GENERALIZATION2_CONDITIONAL_REGION.md
docs/LEVEL4_GENERALIZATION3_LIMITED_TEXTURE2D_FLOW.md
docs/LEVEL4_GENERALIZATION4_VERIFIED_INDIRECT_WORK_EXECUTION.md
docs/LEVEL4_GENERALIZATION5_LIMITED_TEXTURE2D_UAV_COMPUTE_FLOW.md
docs/LEVEL4_GENERALIZATION6_MULTI_TARGET_VERIFIED_DYNAMIC_ROUTING.md
docs/LEVEL4_GENERALIZATION7_VERIFIED_TEMPORAL_BUFFER_FLOW.md
docs/LEVEL4_GENERALIZATION8_VERIFIED_COMPACT_SPARSE_WORKLIST.md
docs/LEVEL5_VERTICAL_EXPERIMENT1_SPARSE_INDIRECT_TEMPORAL_IMAGE.md
docs/LEVEL5_VERTICAL_EXPERIMENT1_RESULT.md
```

## Build

Visual Studio 2026、MSVC v145、Windows SDK 10が必要です。

```bat
build_new_sge4.bat
```

Level 5垂直実験はFull Gateとは別のEvidence Runnerで実行します。

```bat
run_sge4_level5_vertical_experiment.bat
```

## Full Gate

新しいフォルダへ展開して実行してください。

```bat
run_new_sge4_full_gate.bat
```

Full Gateは次を確認します。

- Source Manifest
- Debug／Release build
- Debug A／Debug B／Release Frozen bytes一致
- ABI 2.8 flat Section／round-trip／migration／corruption／Dynamic algebra／multi-route execution payload／Conditional execution／limited Texture2D Flow／verified DispatchIndirect／Verified Compact Worklist／Temporal Buffer Flow
- 40 carried invariants
- WARP materialization／submission／readback
- Controlled whole-composition Recovery
- stale epoch rejection
- Actual Device removal／removed-adapter exclusion

すべてのMSBuild入口は`/nr:false`を使用し、Gate終了後にコマンドラインMSBuild workerを残しません。

## Scope

今回含めたもの:

- Bufferおよび限定Texture2Dのfinite static DAG
- fixed-size Bufferのhistory depth 1 Temporal Flow
- Previous／Current二世代とsuccessful full-submit後だけのatomic rotation
- 明示的Temporal seedとRecovery後のseed再構築
- single writer
- optional single presenter
- single adapter／shared device domain
- direct embedded Schema 17 Leaf packages
- independent Leaf／Composition／Dynamic verification
- exact sparse membership
- activation／deactivation／update／retain／transition
- verified indirect quantity
- verified DispatchIndirect work execution（Compute Leaf／1 Command限定）
- zero-work DispatchIndirectと固定上限契約
- exact Transition setからCanonical uint32 Compact Worklistへの独立導出
- fixed-size index-list Dynamic Slotへのzero padding付きmaterialization
- Dispatch ordinalから任意verified member IDへの写像
- verified dense Dynamic Slot execution
- one Canonical member payloadから複数Leaf／複数Dynamic Slotへのverified byte-slice routing
- 全route shadowのUpdate／Clear一括適用とsubmit成功後の原子的Commit
- non-nested Conditional Region／exact-set predicate
- sealed True／False branch selectionとenabled Leaf集合
- zero-Leaf submission／未選択Resource状態保持
- fixed BGRA8 Texture2D Flow（single mip／layer／plane／sample）
- fixed RGBA32F Texture2D UAV／Compute Flow（single mip／layer／plane／sample）
- RTV producer → SRV consumerおよびUAV producer → SRV consumerのstate／completion handoff
- RGBA32F intermediate／BGRA8 outputのpacked readback
- D3D12 pitch-aware upload／packed readback
- exact Update payload／Clear／Retainの実GPU反映
- submit成功後だけのHistory／shadow同時Commit
- explicit history validity
- epoch-bound handles
- whole-composition recovery
- explicit external-rebind gate
- full-active RecoverySeed

今回追加していないもの:

- Texture2Dのmip／array／MSAA／Depth／任意UAV format／subresource一般化
- Frozen Variant Set
- Streaming／Residency
- Partial Recovery
- Multiple Adapter
- Runtime candidate／performance policy
- Conditional Regionのネスト、任意bool slot、Conditional Presenter
- routeごとの独立membership、Runtime変換、可変長member、GPU生成worklist／scatter
- 一般ExecuteIndirect、複数Indirect target、Dispatch Y／Z可変、GPU生成count、count buffer chain
- Temporal Texture、history depth 2以上、Conditional Temporal writer／reader、partial temporal recovery

## Validation boundary

このLinux環境では、Portable C++23厳格構文検査、ABI 1.x Oracle、ABI 2.8直接生成／Round-trip／Migration／corruption、Canonical Artifact、Migration Acceptance、Project／dependency／source ownership監査、Manifest検証を実施します。MSVC、HLSL、WARP、Actual Device removalの最終合格は、Windows上の`run_new_sge4_full_gate.bat`で確定します。

詳細は次を参照してください。

```text
docs/FROZEN_COMPOSITION_ABI_2_0.md
docs/FROZEN_COMPOSITION_ABI_2_0_MIGRATION.md
docs/CPP23_SOURCE_RECONSTRUCTION_V1.md
docs/INTERNAL_RECONSTRUCTION_V1.md
docs/SOURCE_AUTHORITY_MAP_V1.md
docs/VALIDATION_REPORT.md
docs/KNOWN_LIMITATIONS.md
```
