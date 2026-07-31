# NewSGE4 v1.3.1 `src/internal/` 設計・実装監査と再構築計画 V1

## 0. 結論

`v1.3.1`のFull Gate合格は維持できているが、`src/internal/`はまだ最終Architectureではない。現状は、

1. Level 1〜3の実行可能な旧Leaf／Composition実装、
2. Level 4 v2のidentity中心authority実装、
3. 新しいUnified bridge／Facade

の三層が同時に存在する過渡構造である。

最終方針は、**実行可能な完全Plan／Artifactを正本にし、v2で得たidentity・seal・拒否条件をその正本へ吸収する**ことである。旧実装を捨ててv2 identity-only型へ置き換えるのでも、bridgeを永久保存するのでもない。

今回コード変更は行っていない。`NewSGE4_Unified_Reconstruction_v1.3.1_SolutionFolderFix.zip`を変更不能Baselineとして監査した。

## 1. 調査範囲

- `src/internal/`: 35ディレクトリ、96ファイル、22,083行
- 新Unified source: 6ディレクトリ、12ファイル、1,649行
- 製品Translation Unit、ProjectReference、相対include、公開Headerの内部型露出
- Full Gateの40不変条件とU1〜U12 Acceptance Matrix

分類記号:

- **A**: 最終正本として残す
- **B**: 正しい実装。新責務へ移動・改名する
- **C**: 正本へ吸収する重複層／bridge。移植完了後に削除する
- **D**: Test fixtureへ分離する
- **E**: Referenceとして退役する
- **F**: 代替成立後に削除する

## 2. 現在の実行経路

### Leaf

`CompileFrozenLeaf`は旧`CompileCanonical`で完全Schema 17 Packageを生成した後、Packageを再読込し、`BuildAuthority`で別のv2 Planner／Verifierを走らせてauthority tokenを作る。

- `src/10_LeafToolchain/LeafToolchain.cpp:140` `BuildAuthority`
- `src/10_LeafToolchain/LeafToolchain.cpp:185` 読込後authority再構築
- `src/10_LeafToolchain/LeafToolchain.cpp:209` 旧Compiler実行

したがって、実ExecutionPlanのSealとv2 Authority Sealが並列に存在する。

### Composition

`BuildFrozenCompositionPackage`は旧Contract→旧Plan→旧Verifier→完全Composition bytesを作り、その完成物を再解析してv2 Contract→v2 Planner→v2 Verifierをもう一度走らせる。

- `src/20_CompositionToolchain/CompositionToolchain.cpp:72` `BuildV2CompositionAuthority`
- 同:207 旧Composition Planner
- 同:210 旧Composition Verifier
- 同:220 完成後v2 authority生成
- 同:288 読込時にもv2 authority再生成

### Dynamic

R4のExact set algebraは比較的素直で、201〜204と新serialization wrapperが一つの流れを形成している。ここは主に移動・命名整理でよい。

### Runtime

`SubmitUnified`はraw `InvocationInputV1`を受け、Runtime内部から`PlanAndFreezeInvocation`を呼ぶ。

- `src/40_UnifiedRuntime/UnifiedRuntime.cpp:117`

これはArchitecture Contractの「RuntimeはPlannerではない」「Frozen Dynamic Invocationを消費する」と一致しない。Full Gateは機能を証明したが、責務境界は未完成である。

## 3. 主要な設計上の発見

### F1. Planner→Verifierの依存が残る

`08_CandidatePlanner/CandidatePlanner.h`は`07_ExecutionPlanVerifier`をincludeし、CandidateRecordがVerificationReportを所有する。PlannerがSealを発行できない点は守られているが、厳密な依存方向はPlanner→Verifierである。

最終形では:

```text
Leaf Planner -> Raw Candidate Set
Leaf Compiler/Orchestrator -> Independent Verifier
Independent Verifier -> Opaque Verified Plan
Verified Plan -> D3D12 Lowering
```

とし、PlannerからVerifierへのcompile dependencyを除去する。

### F2. RuntimeがDynamic Compilerを内包する

現在の`40_RuntimeCore`はDynamic Planner／VerifierにProjectReferenceし、`UnifiedRuntime`もraw入力からPlan/Verify/Freezeする。最終形は:

```text
Dynamic Toolchain -> FrozenDynamicInvocation
Runtime -> FrozenDynamicInvocationのみ消費
```

である。便利な「raw inputからsubmit」APIを残す場合もRuntime外のFacadeに置く。

### F3. Public Headerが`internal`型を公開している

- `LeafToolchain.h`: SemanticGraph、D3D12TargetProfile、D3D12 schema state、OpaqueFrozenArtifact
- `CompositionToolchain.h`: legacy Contract、VerifiedFrozenComposition、v2 OpaqueFrozenComposition
- `UnifiedRuntime.h`: D3D12Backend、legacy LoadedStaticComposition、WholeCompositionRecoveryReport

このため`internal/`を物理移動すると公開APIまで連鎖する。最終public APIは新名前空間の型だけを返し、旧型はprivate implementationへ隠す。

### F4. 完全Artifactとidentity-only Artifactが二重化

- Leaf: `FrozenExecutablePackage` + `OpaqueFrozenArtifactV1`
- Composition: `VerifiedFrozenComposition` + `OpaqueFrozenCompositionV1`

最終正本は完全Artifact一つとし、そのManifest／Certificate内にCanonical identity、Verified plan identity、seal、resource contract identityを保存する。

### F5. Artifact処理がVerifierに混在

`19_CompositionVerifier`がVerify/Sealだけでなく`FreezeVerifiedComposition`と`ReadVerifiedFrozenComposition`も所有する。Verifierは判断に限定し、serialization/readbackはComposition Artifactへ移す。

### F6. D3D12 CompilerとD3D12 Executorが分散

- Schema／Loweringは`13_LeafArtifact`
- Executorは`50_D3D12Runtime`
- `50_D3D12Executor` facadeは型Aliasだけ

最終的には`D3D12Compiler`と`D3D12Executor`を別の真の境界にする。これは歴史的分割ではなく、compile-time target loweringとruntime API executionの本質的分離である。

### F7. `03_SemanticBuilder`は製品コードではない

製品14プロジェクトには含まれず、`tests/fixtures/RuntimeFixture.h`から利用される。`tests/fixtures/leaf`へ移す。

## 4. ディレクトリ別Disposition

| 現在 | 分類 | 最終owner | 処置 |
|---|---:|---|---|
| `src/internal/00_Foundation` | B | `src/canonical/base` | 移動・正本化。Result/BinaryIO/FileIO/SHA-256/checked math/StrongIdをCanonical Coreの基盤へ移す。 |
| `src/internal/02_SemanticModel` | B | `src/leaf/model` | 移動・正本化。Leaf内部Semantic Graphの正本。 |
| `src/internal/03_SemanticBuilder` | D | `tests/fixtures/leaf` | 製品経路から分離。現在は製品14プロジェクトに含まれず、試験Fixtureからのみ利用。 |
| `src/internal/04_SemanticAnalysis` | B | `src/leaf/model` | 移動・正本化。Semantic依存・Lifetime解析の正本。 |
| `src/internal/05A_CompilationInput` | B | `src/leaf/model` | 移動・正本化。SemanticとTargetを結ぶ検証済み入力。 |
| `src/internal/05_TargetContract` | B | `src/backends/d3d12/compiler` | 移動・改名。D3D12TargetProfileであり、汎用Leaf ModelではなくTarget Compiler契約。 |
| `src/internal/06_ExecutionPlanModel` | B | `src/leaf/model/plan` | 移動・分割検討。ExecutionPlanIRとObligationはLeaf側。D3D12PlanningContractはTarget Compiler側へ分離候補。 |
| `src/internal/07_ExecutionPlanVerifier` | A | `src/leaf/verifier` | 移動・正本化。実ExecutionPlanを独立検証しSealする中心正本。 |
| `src/internal/08_CandidatePlanner` | B | `src/leaf/planner` | 移動・設計修正。候補生成は残すがVerifier呼出しとVerificationReport所有を除去し、Raw Candidateのみ生成する。 |
| `src/internal/09_FrozenPackageCore` | A | `src/leaf/artifact` | 移動・正本化。完全Frozen Leaf PackageのReader/Writer/Format/Digest。 |
| `src/internal/10_D3D12PackageSchema` | B | `src/backends/d3d12/artifact` | 移動・正本化。D3D12固有Frozen schema。Leaf Artifactの汎用領域から分離。 |
| `src/internal/11_D3D12PackageLowering` | B | `src/backends/d3d12/compiler` | 移動・正本化。Verified Leaf PlanからD3D12 PackageへLowering。 |
| `src/internal/12_SGE4_5Compiler` | C | `src/leaf/toolchain` | 吸収・改名。Leaf Planner/Verifier/Target Loweringを統括する正式LeafCompilerへ吸収。旧Facade名を退役。 |
| `src/internal/13_PackageRuntime` | A | `src/runtime/core` | 移動・正本化。PortableなPackage Runtime interfaceとLeaf instance lifecycle。 |
| `src/internal/14_D3D12Backend` | A | `src/backends/d3d12/executor` | 移動・正本化。実D3D12 Executor。Alias facadeではなく正式入口にする。 |
| `src/internal/16_FrozenCompositionArtifact` | A | `src/composition/artifact` | 移動・正本化。完全Composition ContainerのFormat/Reader/Writer/Digest。 |
| `src/internal/17_CompositionContract` | A | `src/composition/model` | 移動・正本化。Leaf/Endpoint/Flowの完全契約。D3D12型露出は後段で抽象Interfaceへ整理。 |
| `src/internal/189_Level4V2CanonicalVocabulary` | B | `src/canonical/identity` | 移動・統合。CanonicalIdentity/StrongScalar/DeviceEpoch/Handleを共通正本へ。Digest型重複を解消。 |
| `src/internal/18_CompositionPlan` | B | `src/composition/model + src/composition/planner` | ファイル分割。Plan data/serializationはModel、ProposeCompositionPlanはPlannerへ分離。 |
| `src/internal/191_Level4V2AuthorityModel` | C | `src/canonical/authority + src/leaf/model` | 分解・吸収。共通Seal/IdentityはCanonical、Leaf固有Request/ProposalはLeafへ。並列authority pipelineは廃止。 |
| `src/internal/192_Level4V2CandidatePlanner` | C | `src/leaf/planner` | ロジック吸収後削除。実Plan生成後のpost-hoc authority proposalを廃止し、正式Leaf Plannerへ統合。 |
| `src/internal/193_Level4V2IndependentVerifier` | C | `src/leaf/verifier + src/leaf/artifact` | 検査移植後削除。Authority ledger整合性検査を実Plan VerifierとArtifact Readerへ移植。 |
| `src/internal/194_Level4V2FrozenAuthority` | C | `src/leaf/artifact` | 型吸収後削除。Opaque authority tokenを完全FrozenLeafのCertificate/Manifestへ内包。 |
| `src/internal/196_Level4V2CompositionModel` | C | `src/composition/model + src/canonical/identity` | 分解・吸収。backend-neutral identity/invariantを完全Composition Modelへ統合。並列Raw Contractを廃止。 |
| `src/internal/197_Level4V2CompositionPlanner` | C | `src/composition/planner` | 検査統合後削除。簡略identity Planを実Composition Plan生成へ統合。 |
| `src/internal/198_Level4V2CompositionVerifier` | C | `src/composition/verifier` | 検査統合後削除。v2固有拒否条件を完全Composition Verifierへ独立実装として移す。 |
| `src/internal/199_Level4V2FrozenComposition` | C | `src/composition/artifact` | 型吸収後削除。identity-only Frozen Compositionを完全FrozenComposition Certificateへ内包。 |
| `src/internal/19_CompositionVerifier` | B | `src/composition/verifier + src/composition/artifact` | ファイル分割。Verify/SealはVerifier、Freeze/ReadVerifiedFrozenCompositionはArtifactへ移す。 |
| `src/internal/201_Level4V2DynamicInvocationModel` | A | `src/dynamic/model` | 移動・正本化。Exact set/transition/historyの中心モデル。 |
| `src/internal/202_Level4V2DynamicInvocationPlanner` | A | `src/dynamic/planner` | 移動・正本化。Dynamic proposal生成。 |
| `src/internal/203_Level4V2DynamicInvocationVerifier` | A | `src/dynamic/verifier` | 移動・正本化。独立再導出・検証。 |
| `src/internal/204_Level4V2FrozenInvocationHistory` | B | `src/dynamic/artifact` | 移動・正本化。Verified Dynamic Invocation/HistoryのFrozen保持。 |
| `src/internal/20_CompositionDeviceDomain` | B | `src/backends/d3d12/runtime` | 移動・改名。D3D12Backendを直接所有するためPortable RuntimeではなくD3D12 Runtime実装。 |
| `src/internal/21_CompositionSharedResources` | B | `src/backends/d3d12/runtime` | 移動・正本化。D3D12 Shared Resource materialization。 |
| `src/internal/22_CompositionRuntime` | B | `src/backends/d3d12/runtime` | 移動・正本化。実Composition submission。公開APIからlegacy型を隠す。 |
| `src/internal/23_CompositionRecovery` | B | `src/backends/d3d12/runtime/recovery` | 移動・正本化。Whole-composition D3D12 recoveryの実装。 |


### Unified sourceのDisposition

| 現在 | 分類 | 最終owner | 処置 |
|---|---:|---|---|
| `src/00_UnifiedCanonicalCore` | C | `src/canonical/artifact + src/canonical/diagnostics` | 分割・吸収。SectionedArtifactはCanonical Artifactへ。Error/Digest alias/IsPowerOfTwo重複を共通基盤へ統合。 |
| `src/10_LeafToolchain` | C | `src/leaf/toolchain + src/leaf/artifact` | 書き直し・吸収。package読込後にv2 authorityを再構築するbridgeを廃止し、実Verified Planから一度だけCertificateを生成。 |
| `src/20_CompositionToolchain` | C | `src/composition/toolchain + src/composition/artifact` | 書き直し・吸収。legacy complete planとv2 planを二重実行するbridgeを廃止。単一完全Plan/Verifier/Artifactへ。 |
| `src/30_DynamicInvocation` | B | `src/dynamic/artifact + src/dynamic/toolchain` | 移動・吸収。集合代数は保持。Planner/Verifier/Frozen serializationを正式Dynamic toolchainへ配置。 |
| `src/40_UnifiedRuntime` | C | `src/runtime/core + src/backends/d3d12/runtime` | 責務分割・公開API再設計。Runtime内でPlannerを呼ばず、FrozenDynamicInvocationだけを消費。NativeRuntime露出を除去。 |
| `src/50_D3D12Executor` | F | `削除（実装はsrc/backends/d3d12/executor）` | Alias削除。15行の型Aliasのみ。D3D12Backendを正式public executorへ昇格後に不要。 |


## 5. 推奨する最終Source Tree

```text
src/
  canonical/
    base/
    identity/
    authority/
    artifact/
    diagnostics/

  leaf/
    model/
    planner/
    verifier/
    artifact/
    toolchain/

  composition/
    model/
    planner/
    verifier/
    artifact/
    toolchain/

  dynamic/
    model/
    planner/
    verifier/
    artifact/
    toolchain/

  runtime/
    core/
    session/
    recovery_contract/

  backends/
    d3d12/
      compiler/
      artifact/
      executor/
      runtime/
      recovery/
```

`internal/`、旧Level番号、R番号はActive pathから消す。来歴はGit historyと`docs/INTERNAL_RECONSTRUCTION_MAP_V1.md`で保持する。

## 6. 推奨Project Boundary

現在の14製品Projectを固定目標にしない。監査の結果、D3D12 CompilerとExecutorは分ける価値があるため、推奨は15製品Projectである。

```text
01_CanonicalCore

10_LeafModel
11_LeafVerifier
12_LeafPlanner
13_LeafArtifactToolchain

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

Planner／Verifierは別Projectのまま維持する。Toolchainを別Static Libraryに増やさず、Artifact側にOrchestratorを置くことで数を抑える。ただしRuntimeはPlanner／Verifierへ依存しない。

## 7. 目標依存方向

```text
CanonicalCore

LeafModel
  ├─> LeafPlanner
  └─> LeafVerifier
LeafPlanner + LeafVerifier + D3D12Compiler
  └─> LeafArtifactToolchain

LeafArtifactToolchain
  └─> CompositionModel
CompositionModel
  ├─> CompositionPlanner
  └─> CompositionVerifier
CompositionPlanner + CompositionVerifier
  └─> CompositionArtifactToolchain

CompositionArtifactToolchain
  └─> DynamicModelArtifact
DynamicModelArtifact
  ├─> DynamicPlanner
  └─> DynamicVerifier
DynamicPlanner + DynamicVerifier
  └─> DynamicModelArtifact freeze entry

CompositionArtifactToolchain + DynamicModelArtifact
  └─> RuntimeCore

RuntimeCore + D3D12 artifact/schema
  └─> D3D12Executor
```

禁止依存:

- Planner → Verifier
- Runtime → Planner／Verifier
- Canonical → D3D12
- Composition public model → concrete D3D12Backend
- Public API → `src/internal/*`

## 8. 正本統合の具体策

### 8.1 Leaf

1. `ExecutionPlanIR`を実Plan正本として維持。
2. PlannerはRaw Candidateだけ生成。
3. Verifierだけが`VerifiedLeafPlan`を構築。
4. D3D12 Loweringは`VerifiedLeafPlan`のみ受け取る。
5. Package Manifestへv2のSemantic/Target/Resource/WriteSet/Plan/Seal identityを直接保存。
6. ReaderはPackage bytesからCertificateを検証し、`OpaqueFrozenArtifactV1`を別途再生成しない。
7. 191〜194のrejection scenarioをLeafVerifier／ArtifactReader testsへ移した後、191〜194を削除。

### 8.2 Composition

1. 17〜19の完全Contract/Plan/Verifierを実行正本として維持。
2. 196のbackend-neutral identityと禁止条件を完全Contract/Planへ統合。
3. Plannerは一つだけ。完全Schedule/Allocation/Handoff/Syncを生成し、同時にcanonical identityを計算。
4. VerifierはPlanner helperを呼ばず独立再導出。
5. Frozen Compositionは完全PlanとCertificateを一体保存。
6. 197〜199と`BuildV2CompositionAuthority` bridgeを削除。

現行`SGE4UNI` format major 1はFull Gate済みBaselineとしてReaderを残す。新writerを直接構造のformat major 2にする場合、v1→v2 migration readerとbytes/effect equivalence gateを別に置く。Source整理とABI flatteningは同じコミットで行わない。

### 8.3 Dynamic

201〜204の数理・検証は正本として残す。新wrapperを正式Artifactへ吸収し、次を分離する。

```text
Plan(raw input) -> proposal
Verify(raw input, proposal, previous history) -> verified
Freeze(verified) -> FrozenDynamicInvocation
Runtime Submit(FrozenDynamicInvocation, explicit leaf bytes)
```

### 8.4 Runtime

1. `SubmitUnified`から`PlanAndFreezeInvocation`を除去。
2. Runtime入力を`FrozenDynamicInvocationPackage`へ変更。
3. RuntimeはComposition identity、DeviceEpoch、History identity、Invocation modeの整合だけを検査。
4. `NativeRuntime()` public accessorを削除。Qualification専用friend/adapterへ移す。
5. Portable session stateとD3D12 materializationを分離。
6. Whole-composition Recovery、epoch advance、history invalidation、external rebind、RecoverySeedはRuntime state machineの正本として保持。

### 8.5 D3D12

- Compiler: TargetProfile、Schema、Encoding、Lowering、HLSL compile
- Executor: DeviceDomain、Package instance、Composition shared resources、submission、readback、recovery

`D3D12Executor.h`のAliasは削除し、現`D3D12Backend`を正式`Executor` APIへ改名・移動する。

## 9. 実施順序

### Phase 0: Baseline Freeze

- v1.3.1 ZIP、SHA-256、Full Gate log、環境情報を変更不能保存。
- この監査文書とCSVを追加するがBaseline ZIPは上書きしない。

### Phase 1: 物理移動のみ

- 00/02/04/05A/06/07/09/10/11/13/14/16/17/201〜204/20〜23を新Treeへ移動。
- Namespace、型、ABI、ロジックは変更しない。
- `03_SemanticBuilder`をtestsへ移動。
- Full Gate必須。

### Phase 2: Public Boundary Cleanup

- Public Headerから`internal` includeとlegacy namespace aliasを除去。
- Facade return型を新正本型へ変更。
- NativeRuntime accessorをqualification-onlyへ。
- Full Gate＋public include audit。

### Phase 3: Leaf Authority Unification

- 08からVerifier依存を除去。
- 191〜194の事実を実Plan/Certificateへ吸収。
- post-hoc `BuildAuthority`を削除。
- U1、mutation rejection、Package bytes determinismを固定。

### Phase 4: Composition Authority Unification

- 18/19を責務分割。
- 196〜199の事実を完全Plan/Artifactへ吸収。
- `BuildV2CompositionAuthority`二重経路を削除。
- U2/U4、single writer、presenter、recovery set拒否を固定。

### Phase 5: Dynamic/Runtime Boundary Repair

- raw Invocation planningをRuntime外へ移動。
- RuntimeはFrozen InvocationだけをSubmit。
- U5/U8/U10/U11を再実行。

### Phase 6: D3D12 Boundary Split

- D3D12CompilerとD3D12Executorを分離。
- Composition Runtime具体実装をD3D12側へ移動。
- Portable RuntimeからWindows/D3D12 includeを除去。
- WARP、Controlled Recovery、Actual Removal必須。

### Phase 7: 重複削除と名称固定

削除候補:

- 192, 193, 194
- 197, 198, 199
- 旧形の191/196（抽出後）
- 旧Unified bridge実装
- Aliasだけの50_D3D12Executor
- 空になった`src/internal/`

削除は各不変条件が新ownerのテストへ移ったことをMigration Acceptanceで確認した後だけ行う。

### Phase 8: Optional ABI Flattening

現行SGE4UNI v1 Readerを残し、新format major 2 writerを追加する別作業。これはSource再構築完了後に行う。

## 10. 各Phaseの合格条件

毎Phase共通:

- Debug/Release build
- Debug A/Debug B/Release canonical bytes一致
- Architecture tests
- 40 invariant Migration Acceptance
- Source Manifest
- ProjectReference DAG
- internal path include count

Windows境界変更時:

- WARP materialization/submission/readback
- Controlled Recovery epoch advance
- stale handle rejection
- External rebind gate
- RecoverySeed
- actual Device removal
- AwaitingAdapter preservation
- MSBuild node termination

## 11. 削除判断

現時点で即時削除してよいActive C++実装はない。`D3D12Executor` Aliasでさえ、正式Executor public entryを作ってから削除する。

最終削除条件は:

1. 新ownerへ型・ロジック・negative testが移植済み。
2. 旧HeaderをincludeするActive fileが0。
3. 旧Namespaceを参照するActive symbolが0。
4. 40不変条件が全てReproduced。
5. Full Gateが通る。
6. Source Manifestを再生成済み。

## 12. 最終判断

`src/internal/`見直しは、フォルダ移動だけでは終わらない。中心課題は次の三つである。

1. **実Planとv2 authorityの二重正本を一つにする。**
2. **RuntimeからDynamic Planner/Verifierを追い出す。**
3. **D3D12 CompilerとD3D12 Executorを本当の境界として分離する。**

この順序で進めれば、現在証明済みの範囲を維持したまま、`internal/`と歴史的番号を消し、新SGE4そのものが設計を直接表すSource Treeへ移行できる。
