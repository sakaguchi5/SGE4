# New SGE4 — Unified Two-Stage Compiler Reconstruction

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

Compositionは、ContractからPlanを一度だけ提案し、独立VerifierでSealし、平坦な`SGE4UNI 2.1`へFreezeします。`CompositionCertificate`は、ABI 2.1 Composition Core、検証済みContract、Plan、Seal、Schedule、Recovery Setから直接決定されます。identityだけの第二Composition経路はありません。

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
SGE4UNI Frozen Composition Package 2.1
  Manifest schema 2
  Leaf Table schema 1
  complete Schema 17 Leaf Package bytes
  Contract Data schema 1
  Verified Decision Data schema 1
  Verification Certificate schema 1
  Authority Ledger schema 2
  Dynamic Contract schema 2
```

`CompleteComposition` Sectionと内側`SGE4CMP 1.0`はProduction ABIから廃止しました。Leaf Packageの独立ABIはSchema 17のまま維持し、Leaf bytesを再符号化せず完全に埋め込みます。

Production Readerは`SGE4UNI 2.1`だけを受理します。`SGE4UNI 1.1`／`SGE4CMP 1.0` Reader／Writerは`src/composition/migration/abi1/`へ隔離され、明示的な資格試験用Migration Toolだけが使用します。

Dynamic Invocationは別の`SGE4INV` major 1／minor 2成果物です。Execution Payload Sectionが、Compositionに固定されたLeaf／Dynamic Slot route、member byte幅、exact Update payloadとそのidentityを所有します。Active、Modified Survivor、前History identity、Device epochを明示的にbindし、Activation、Deactivation、Update、Retain、Transition、Indirect quantityをPlannerと独立Verifierが確定します。Runtimeは、ABI 2.1 Composition identityおよび受理済みHistory identityと一致する成果物だけをSubmitできます。

詳細は次を参照してください。

```text
docs/FROZEN_COMPOSITION_ABI_2_0.md
docs/FROZEN_COMPOSITION_ABI_2_0_MIGRATION.md
docs/LEVEL4_GENERALIZATION1_VERIFIED_DYNAMIC_EXECUTION.md
```

## Build

Visual Studio 2026、MSVC v145、Windows SDK 10が必要です。

```bat
build_new_sge4.bat
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
- ABI 2.1 flat Section／round-trip／migration／corruption／Dynamic algebra／verified execution payload
- 40 carried invariants
- WARP materialization／submission／readback
- Controlled whole-composition Recovery
- stale epoch rejection
- Actual Device removal／removed-adapter exclusion

すべてのMSBuild入口は`/nr:false`を使用し、Gate終了後にコマンドラインMSBuild workerを残しません。

## Scope

今回含めたもの:

- Buffer-only finite static DAG
- single writer
- optional single presenter
- single adapter／shared device domain
- direct embedded Schema 17 Leaf packages
- independent Leaf／Composition／Dynamic verification
- exact sparse membership
- activation／deactivation／update／retain／transition
- verified indirect quantity
- verified dense Dynamic Slot execution
- exact Update payload／Clear／Retainの実GPU反映
- submit成功後だけのHistory／shadow同時Commit
- explicit history validity
- epoch-bound handles
- whole-composition recovery
- explicit external-rebind gate
- full-active RecoverySeed

今回追加していないもの:

- Texture Flowの一般化
- Conditional Region
- Frozen Variant Set
- Streaming／Residency
- Partial Recovery
- Multiple Adapter
- Runtime candidate／performance policy
- ExecuteIndirect／可変Dispatchによるwork量自体の省略（Generalization 1では固定Leaf dispatch）

## Validation boundary

このLinux環境では、Portable C++23厳格構文検査、ABI 1.x Oracle、ABI 2.1直接生成／Round-trip／Migration／corruption、Canonical Artifact、Migration Acceptance、Project／dependency／source ownership監査、Manifest検証を実施します。MSVC、HLSL、WARP、Actual Device removalの最終合格は、Windows上の`run_new_sge4_full_gate.bat`で確定します。

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
