# L4V1-F3 Schema 18 Composition Contract 再構築

## 1. 完成範囲

F3は、F1で確立したSchema 18 Composition Endpointと、F2で凍結した54 Leaf Corpusを入力として、Composition全体の構造契約を再構築する段階である。

F3が完成させるものは次の3点である。

1. Schema 18 Leaf Packageだけを受理するComposition Contract Builder
2. Leaf Endpointを唯一の方向・形状・状態・同期権威とするResource Flow Contract
3. 宣言順序に依存しないcanonical contract bytesとcontract identity

F3はResource割当、queue handoff、barrier、signal point、実行順序を計画しない。これらはF4 Resource Flow Planner以降の責務である。

## 2. Prototype-Aから削除した権威

Prototype-AではComposition作者が次を申告できた。

- Import / Export / ImportExport方向
- Shared Resourceのsize、format、initial state
- Endpointとexternal slotの方向対応

F3ではこれらをContractBuildInputから除去した。

作者が指定できるものは次だけである。

- Leaf instanceのstable key
- Schema 18 Leaf Package bytes
- Resource Flowのstable key
- どのLeaf Endpointを同じResource Flowへ所属させるか
- Resource FlowがInternal、CompositionInput、CompositionOutputのどれか

Producer / Consumer方向はEndpointAccessからのみ導出される。

- WriteOnly Endpointだけがproducerになれる
- ReadOnly Endpointだけがconsumerになれる
- ReadWriteはSchema 18時点ですでに存在できない

Resource kind、format、minimum size、required incoming state、guaranteed outgoing state、synchronization contractもLeaf Packageから取得され、作者は上書きできない。

## 3. Resource Flow規則

### Internal

- producer: 1個必須
- consumers: 1個以上必須
- producerはWriteOnly
- consumersはすべてReadOnly

### CompositionInput

- producer: 禁止
- consumers: 1個以上必須
- consumersはすべてReadOnly

### CompositionOutput

- producer: 1個必須
- consumers: 禁止
- producerはWriteOnly

各Schema 18 Endpointは、ちょうど1つのResource Flowへ所属しなければならない。未接続と二重接続は拒否される。

Resource FlowのsizeBytesは、所属EndpointのminimumBytes最大値として導出される。kindとformatは全Endpointで一致しなければならない。

## 4. Canonical identity

Leafはstable leaf key digest順、EndpointはLeaf IDとstable endpoint key順、Resource Flowはstable resource key digest順へ並べ替えられる。

そのため、以下を入れ替えてもContract bytesは変わらない。

- Leaf declaration順
- Resource Flow declaration順
- consumer declaration順

Canonical serializationにはLeaf Package bytesそのものが含まれる。contract identityはcanonical bytesのSHA-256である。

## 5. Presenter制約

Level 4 v1ではComposition全体でSurface slotを持つLeafは最大1つに制限する。Presenter Leafは作者申告ではなく、Leaf PackageのSchema 17 baseから抽出される。

## 6. F3代表資格シナリオ

F2 Corpusから次を使用する。

- producer: StageK/DynamicExternalDedicated
- consumer instance A: StageK/DynamicExternalDirectFallback
- consumer instance B: 同一Packageの別Leaf instance

構造は次のとおりである。

```text
Composition Input
      |
      v
Producer Leaf
      |
      +------------------+
      v                  v
Consumer A           Consumer B
      |                  |
      v                  v
Composition Output A  Composition Output B
```

固定構成数は次のとおり。

```text
Leaves:         3
Endpoints:      6
Resource Flows: 4
Bindings:       6
Presenter:      none
```

## 7. Negative gates

46Cテストは少なくとも次を拒否する。

- Schema 17 PackageをLeafとして渡す
- required Endpointの未接続
- Endpointの二重接続
- WriteOnly Endpointをconsumerとして使用
- ReadOnly Endpointをproducerとして使用
- 存在しないstable endpoint key
- duplicate leaf key / resource key
- boundaryとproducer / consumer形状の不一致
- Package bytes改ざん
- Contract object内のEndpoint access改ざん
- Endpointから導出したResource sizeの改ざん
- 2つのPresenter Leaf

## 8. 実行

```powershell
.\prepare_sge4_level4v1_f3.bat
.\run_sge4_level4v1_f3.bat
```

F3用Solutionは次である。

```text
SemanticGpuEngine4_Level4v1_F3.sln
```

RunnerはProjectReferenceのPlatformフォールバックを避けるため、46A、46B、46CのvcxprojをPlatform=x64で直接ビルドする。

## 9. 次段階

F4 Resource Flow Plannerは、このF3 Contractを入力として次を導出する。

- physical shared resource identity
- producer / consumer間の実行順序
- queue間handoff
- required incoming / guaranteed outgoing stateを満たすtransition
- signal / wait候補

F4はF3 Contractの方向や形状を変更してはならない。
