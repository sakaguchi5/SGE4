# SGE4 L4V1-F7〜F9 — Shared DeviceDomain / Shared Resource / Static DAG Runtime

## 1. この統合工程の目的

F1〜F6で、Schema 18 Leaf、Composition Contract、Resource Flow Plan、独立Verifier、Frozen Compositionまでが静的に固定された。
F7〜F9は、そのFrozen CompositionをD3D12/WARP上で実行する最初のRuntime工程である。

この工程ではRecoveryを実装しない。Device loss、epoch更新、再物質化、AwaitingAdapterはF10の責務として残す。

## 2. F7 — Shared DeviceDomain

`13A_Schema18SharedDeviceDomain`は、F6のFrozen Composition bytesだけを入力として受け取る。
構造体を信用せず、最初に`ReadFrozenComposition`でArtifact、Contract、Plan、Verifier sealを再検証する。

Schema 18 Leaf PackageはComposition authorityであり、既存Runtimeの直接入力ではない。そのため各Leafから埋め込みSchema 17 base packageを抽出し、全Leafを同じ`IPackageDeviceDomain`へロードする。

- Schema 18: stable endpoint、access、shape、state、同期の権威
- Schema 17: 実証済みD3D12実行ABI
- DeviceDomain: 共有D3D12 deviceとdevice epochの所有者

Presenterを持つCompositionでは、Contractが指定する唯一のPresenter Leafだけに`ISurfaceHost`を渡す。

## 3. F8 — Composition-owned Shared Buffer

`14A_Schema18SharedResources`は、Verified Planのallocationごとに1個のnative BufferをDeviceDomain内へ作成する。
同じResource Flowへbindされたproducer/consumer Endpointは、Leafやexternal slotが異なっても同じnative Buffer identityを共有する。

各Resource Flowは次をComposition側で所有する。

- native Buffer
- device epoch
- current ResourceState
- 最新のcompletion token
- endpoint-to-resource mapping

Schema 18のWriteOnly EndpointとReadOnly Endpointでは、要求stateが異なる。そこでBackendへ最小API`TransitionSharedBuffer`を追加し、前のcompletion tokenを待ってからComposition-owned transitionを実行する。

Transitionは次を検証する。

- Resourceとtokenが同じDeviceDomain所有である
- Resourceとtokenのepochが現在epochと一致する
- Resource identityとtoken identityが一致する
- Runtimeが記録したbefore-stateとnative resource stateが一致する

## 4. F9 — Static Package DAG Runtime

`29A_Schema18StaticDagRuntime`は、Frozen Composition内のcanonical scheduleだけを実行する。
RuntimeはCompiler、Planner、Verifier、LinkerのFreeze操作を呼ばない。F6のArtifact readerだけをロード境界として使用する。

各Leaf実行では次を行う。

1. Frozen scheduleから次のLeafを選ぶ
2. Verified bindingからexternal slotとResource Flowを解決する
3. Endpoint required incoming stateへResourceを準備する
4. 最新completion tokenをLeafへ渡す
5. Schema 17 base Packageをsubmitする
6. 全required Endpointがreleaseされたことを確認する
7. release tokenとguaranteed outgoing stateをResource Flowへ戻す

fan-out consumerはcanonical schedule順にretireする。Resource Flowの最新tokenは、producerとそれ以前のreaderを推移的に支配する。これにより次frameのwriterは最後のconsumerまで完了した後に再利用できる。

## 5. 資格試験

- `46G_Schema18SharedDeviceDomainTests`
  - 4 Leafを1 DeviceDomainへロード
  - 全Leafが同一epoch
  - Schema 18 authorityからSchema 17 execution packageを抽出
  - Presenterのsurface欠落を拒否
  - Frozen bytes改ざんを拒否

- `46H_Schema18SharedResourceTests`
  - 5 Resource Flowをnative Bufferへ物質化
  - fan-outの3 Endpointが同じnative Bufferを共有
  - Resource/tokenのepoch authority
  - initial-data WARP readback
  - duplicate/oversized initial dataを拒否

- `46I_Schema18StaticDagRuntimeTests`
  - independent DAG
  - linear dependency
  - fan-out
  - two-input merge
  - diamond DAG
  - headless composition
  - single-presenter composition
  - repeated frameによるfan-out retirement authority
  - Frozen bytes改ざんと重複dynamic bindingを拒否
  - Debug別processおよびDebug/Release evidence manifest byte identity

Diamondの期待値は、入力`{1,2,3,4}`に対して`{7,9,11,13}`である。

## 6. 非目標

F7〜F9では以下を完成扱いにしない。

- whole-domain recovery
- actual RemoveDevice
- stale epoch/resource/token rejection after recovery
- temporal resetとexternal rebind
- fresh-process recovery rematerialization
- Level 4 v1 Final Freeze

これらはF10〜F12で閉じる。
