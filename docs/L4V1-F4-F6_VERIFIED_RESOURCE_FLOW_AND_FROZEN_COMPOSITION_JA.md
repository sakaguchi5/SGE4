# SGE4 Level 4 v1 F4〜F6
## Verified Resource Flow Plan and Frozen Composition

基準コミット: `547ed45695e8f6b15f0e0d5d5be5992d20bb8134`

## 1. この統合工程の目的

F1〜F3で、Schema 18 Leaf Packageが自身のEndpoint authorityを持ち、Composition ContractがそのEndpointだけから構築されるようになった。

F4〜F6では、Contractを実行可能な静的構成へ近づける。ただし、この工程ではRuntimeやD3D12 Backendを実行しない。責務は次の3段階に固定する。

1. F4: ContractからRaw Resource Flow Planを決定的に提案する。
2. F5: Plannerとは独立したVerifierがPlanを再導出し、正しいPlanだけをSealする。
3. F6: Verified Planだけを、自己完結したFrozen Composition Artifactへ固定する。

権威チェーンは次のとおりである。

```text
Schema 18 Leaf Packages
        ↓
Schema 18 Composition Contract
        ↓
Raw Resource Flow Plan          F4: proposal only
        ↓
Independent VerifyAndSeal       F5: authority boundary
        ↓
VerifiedResourceFlowPlan
        ↓
Frozen Composition Artifact     F6: immutable runtime input
```

Raw Planを直接Freezeまたは実行するAPIは存在しない。

## 2. F4 — Resource Flow Plan Model / Planner

追加プロジェクト:

```text
17A_Schema18ResourceFlowPlan
46D_Schema18ResourceFlowPlanningTests
```

Raw Planは次の決定を表現する。

- Resource Flowごとのallocation requirement
- Composition-owned / External Input / External Outputのownership
- Leaf Packageのcanonical topological schedule
- Schema 18 EndpointからResource Flowへのbinding
- producer outgoing stateからconsumer incoming stateへのhandoff
- internal Resource Flowごとのsignal point
- consumerごとのwait point
- DeviceDomain再構築時のLeaf / composition-owned Resource identity
- Temporal resetとExternal rebind要求

PlannerはContractの内部Resource FlowからLeaf DAGを構築する。複数の正しいtopological orderが存在する場合も、canonical Leaf IDが小さいものを先に選び、結果を一意にする。

循環するComposition ContractはF4で拒否される。

Raw Planはデータモデルにすぎない。`Execute`、`Submit`、`Freeze`の経路を持たない。

## 3. F5 — Independent Resource Flow Verifier

追加プロジェクト:

```text
18A_Schema18ResourceFlowVerifier
46E_Schema18ResourceFlowAuthorityTests
```

VerifierはPlannerの出力を信用しない。Composition Contractから以下を独立に再導出する。

- allocationのidentity、ownership、kind、format、size
- canonical topological schedule
- Endpoint bindingのLeaf、External Slot、ReadOnly / WriteOnly authority
- state handoffのproducer、consumer、incoming / outgoing state
- signal ownerとschedule ordinal
- wait対象signalとconsumer schedule ordinal
- recovery対象Leafとcomposition-owned Resource
- Contract identityとRaw Plan identity

一致した場合だけ、private constructorを持つ`VerifiedResourceFlowPlan`を生成する。

Verifier Sealは次の組からdomain-separated SHA-256として生成される。

```text
Verifier schema version
Verifier domain identity
Composition Contract identity
Raw Resource Flow Plan identity
```

作者やPlannerはVerified型を直接構築できない。

Negative gateは、攻撃者がRaw Planを変更した後にRaw Plan identityを再計算した場合も拒否する。対象にはallocation、schedule、binding、state、signal / wait、recovery、Contract identityが含まれる。

## 4. F6 — Frozen Composition Artifact

追加プロジェクト:

```text
19A_Schema18CompositionLinker
46F_Schema18FrozenCompositionTests
```

`FreezeVerifiedComposition`は次の2引数だけを受け取る。

```text
Schema 18 Composition Contract
VerifiedResourceFlowPlan
```

Raw Planを直接渡すoverloadは存在しない。

Frozen Compositionには次が埋め込まれる。

- canonical Composition Contract
- すべてのSchema 18 Leaf Package bytes
- canonical Raw Resource Flow Plan
- Contract identity
- Plan identity
- independent Verifier Seal
- outer Artifact digest

ReaderはCompilerとPlannerを呼ばない。ArtifactからContractとPlanを復元し、次を再検証する。

1. outer size / magic / version
2. Artifact digest
3. embedded Contractのcanonical structureとidentity
4. embedded Leaf Package bytesとEndpoint authority
5. Raw Planのcanonical encodingとidentity
6. Contractに対するIndependent Verifier qualification
7. Verifier Seal

したがって、後続F7〜F9のRuntimeは、Compiler、Planner、Linker proposal logicを持たずにFrozen Compositionをロードできる。

## 5. 代表資格シナリオ

F3と同じ3 Leaf / 4 Resource Flow contractを使用する。

```text
Composition Input
        ↓
Producer Leaf
        ↓
Composition-owned shared Buffer
        ├──→ Consumer Leaf A ──→ Composition Output A
        └──→ Consumer Leaf B ──→ Composition Output B
```

期待されるRaw Plan:

```text
Leaves:       3
Allocations:  4
Bindings:     6
Handoffs:     2
Signals:      1
Waits:        2
Recovery-owned Resources: 1
```

別の2 Leaf contractではA→BとB→Aを同時に接続し、F3 ContractとしてはEndpoint authorityを満たしていても、F4 Plannerがcycleとして拒否することを確認する。

## 6. 決定性

Runnerは次を比較する。

```text
Raw Plan Debug process A
Raw Plan Debug process B
Raw Plan Release

Frozen Composition Debug process A
Frozen Composition Debug process B
Frozen Composition Release
```

それぞれbyte-for-byteで一致しなければ失敗する。

## 7. 実行方法

リポジトリ直下で実行する。

```powershell
.\prepare_sge4_level4v1_f4_f6.bat
.\run_sge4_level4v1_f4_f6.bat
```

Visual Studioで参照する場合:

```text
SemanticGpuEngine4_Level4v1_F4_F6.sln
```

既存のメインSolution、F1、F2、F3 Solutionは変更しない。

## 8. この工程で証明しないもの

F4〜F6では次を実装・証明しない。

- D3D12 DeviceDomain
- native shared Bufferの作成
- cross-Package Queue/Fence実行
- WARP readback
- Static DAG Runtime
- Device loss / whole-domain recovery

これらはF7〜F10の責務である。
