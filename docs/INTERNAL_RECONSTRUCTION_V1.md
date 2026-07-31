# Internal Source Reconstruction v1.4

> Status note: 本文はv1.4時点のSource reconstruction記録である。第9章で別作業として延期したmajor 2平坦化は、Frozen Composition ABI 2.0で完了した。現行Production形式は`SGE4UNI 2.0`であり、詳細は`FROZEN_COMPOSITION_ABI_2_0.md`を参照する。

## 1. Baseline

`NewSGE4 Unified Reconstruction v1.3.1`はWindows Full Gateを通過した変更不能Baselineである。v1.4はその意味、Frozen bytesのmajor互換、Planner／Verifier境界、Runtime観測結果を維持しながら、Active source内部の過渡構造を解消する。

## 2. 解消した構造

v1.3.1では、次の三層が共存していた。

```text
旧Level 1〜3／Level 4 v1の完全実行実装
  -> Level 4 v2のidentity-only authority実装
  -> Unified Facade
```

v1.4では、完全なPlan／Artifactを唯一の実行正本とし、v2のidentity、seal、negative conditionをその正本から直接生成する。

## 3. Leaf

### 変更前

```text
旧ExecutionPlan -> Schema 17 Package
                    -> 再解析
                    -> v2 Raw Candidate
                    -> v2 Independent Verifier
                    -> v2 FrozenAuthority
```

### 変更後

```text
Semantic Graph
  -> Leaf Planner proposal
  -> Independent Leaf Verifier
  -> verified complete Execution Plan
  -> Schema 17 Frozen Leaf Package
  -> LeafCertificate(package factsから直接生成)
```

Leaf PlannerはVerifierを呼ばない。Toolchain orchestratorがPlanner proposalをVerifierへ渡す。`LeafCertificate`はPackage execution、target、resource contract、write set、operation sequenceを決定的にbindする。

## 4. Composition

### 変更前

```text
Level 4 v1 complete Composition
  -> 完全bytesを再解析
  -> v2 Composition Planner
  -> v2 Composition Verifier
  -> identity-only FrozenComposition
```

### 変更後

```text
Composition Contract
  -> Composition Planner proposal
  -> Independent Composition Verifier
  -> complete Verified Frozen Composition
  -> CompositionCertificate
  -> SGE4UNI Frozen Composition Package
```

Contract、Plan、Schedule、Recovery Set、Sealは一つの完全Compositionから直接決定される。

## 5. Dynamic Invocation

Dynamic compilationとRuntime submissionを分離した。

```text
InvocationInput
  -> BuildDynamicInvocationRequest
  -> Dynamic Planner
  -> Independent Dynamic Verifier
  -> FreezeVerifiedInvocation
  -> FrozenDynamicInvocationPackage
```

Runtime CoreはDynamic Planner／Verifierへ依存しない。Runtimeが受け取るのは検証済みFrozen Invocationだけである。ContinueHistory成果物はVerifierが使用した前History identityを保持し、Runtime Sessionが現在受理しているHistory identityとの一致をSubmit前に確認する。

## 6. Runtime

`runtime::Session`がPortable authorityを所有する。

- Frozen Composition identity
- Device epoch
- Representation／History Handle
- accepted History
- Running／AwaitingAdapterなどの状態
- External Rebind gate
- RecoverySeed requirement

D3D12側の`LoadedComposition`はPIMPLでnative runtime detailを隠し、公開APIは`LoadComposition`、`Submit`、`Recover`、`AcknowledgeExternalRebind`、`ReadBuffer`に限定する。

## 7. D3D12境界

`13_LeafArtifact`はCompilerとExecutorの双方が共有するSchema 17のFrozen schema／encodingを所有する。`D3D12Compiler`はTarget profile、Verified PlanからSchema 17へのLowering、HLSL compile、Leaf compile orchestrationを所有する。`D3D12Executor`は固定済みPackage／CompositionをAPI呼出しへ写像する。ExecutorはQueue、Resource state、Allocation、Composition Flow、Dynamic membershipを再判断しない。

## 8. Source disposition

Active sourceから削除したものは、単純廃棄していない。

```text
reference/retired_source/v2_leaf_authority/
reference/retired_source/v2_composition_authority/
reference/retired_r5_runtime_reference/
```

これらはSource lineageと監査のための参照であり、Active projectから参照されない。

## 9. ABI

Source reconstructionとABI major更新を同時に行わない。

- Leaf Package: Schema 17を維持
- inner Composition:既存完全Composition formatを維持
- outer Composition: `SGE4UNI` major 1、minor 1
- Dynamic Invocation: `SGE4INV` major 1／minor 1

major 2への平坦化は、新能力ではなくても別のABI migrationとして扱う。

## 10. 完成条件

- `src/internal/`なし
- Active Source pathに旧Level／R番号なし
- Active namespaceに`v2`、`unified`、旧世代名なし
- Leaf／Compositionの二重authority経路なし
- RuntimeからPlanner／Verifier呼出しなし
- D3D12 Compiler／Executor分離
- Composition VerifierとArtifact Freeze／Readの分離
- 15 Product project／3 Qualification project
- 40 carried invariantの所有先維持
- Debug A／Debug B／Release byte一致
- WARP／Controlled Recovery／Actual Removal Full Gate通過
