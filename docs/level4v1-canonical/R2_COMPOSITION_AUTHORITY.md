# SGE4 Level 4 v1 Canonical Reconstruction
## R2 — Composition Contract and Independent Verification

## 1. 目的

R1はFrozen Compositionの固定幅Container、埋込みSchema 17 Leaf、opaqueなContract／Decision／Certificate領域を成立させた。

R2はこの3領域を意味付きの権威チェーンへ置き換える。

```text
validated Schema 17 Leaf Packages
        ↓
PackageCompositionContract
        ↓
RawCompositionPlan proposal
        ↓
independent VerifyAndSeal
        ↓
VerifiedCompositionPlan + VerificationCertificate
        ↓
FreezeVerifiedComposition
        ↓
R1 Frozen Composition Container
```

## 2. Composition Contract

作者が指定できるのは次だけである。

- Leafの安定キー
- 各Schema 17 External slotに対応するEndpoint安定キー
- Resource Flowの安定キー
- Endpoint間の接続
- Internal／CompositionInput／CompositionOutputという境界種別

作者はEndpointのReadOnly／WriteOnly、Resource ID、kind、format、minimum bytes、incoming/outgoing state、synchronization、required flag、Presenterを指定できない。

これらは埋込みSchema 17 Packageを既存PackageReaderとD3D12PackageViewで検証してから再導出する。

Level 4 v1ではComposition-visible endpointを次へ限定する。

- External Buffer
- unformatted
- Required
- CompletionTokenRequired
- ShaderResource viewだけを持つ場合はReadOnly
- UnorderedAccess viewだけを持つ場合はWriteOnly
- ReadWriteまたは混在viewは拒否

任意HLSLの副作用を推論するものではない。権威は既存Level 1-3が固定したProgram Interface、ResourceUse、View artifact、External slot contractである。

## 3. Resource Flow

- Internal: exactly one WriteOnly producer and one or more ReadOnly consumers
- CompositionInput: no producer and one or more ReadOnly consumers
- CompositionOutput: exactly one WriteOnly producer and no consumers
- every required Endpoint belongs to exactly one Resource Flow
- kind／formatは全Endpointで一致
- sizeBytesは参加EndpointのminimumBytes最大値
- Resource Flow、Leaf、Endpointはstable keyからcanonical orderへ変換
- PresenterはLeaf Surface slotから導出し、zero or oneだけを許可

## 4. Raw Plan

RawCompositionPlanはPlannerの提案であり、実行権威ではない。

Plannerは次を提案する。

- allocation ownership／shape／size
- canonical topological Leaf schedule
- Endpoint binding
- producer outgoing stateからconsumer incoming stateへのhandoff
- internal Resource Flowのsignal／wait
- whole-composition recovery identities

Raw Plan APIにはExecute、Submit、Freezeを置かない。

## 5. Independent Verifier

VerifierはContractからすべての表現済み決定を再導出する。

- PlannerのProposeCompositionPlanを呼ばない
- allocation
- schedule
- binding
- handoff
- signal
- wait
- recovery set
- Contract identity
- Raw Plan identity

全項目が一致した場合だけ、private constructorを持つVerifiedCompositionPlanを生成する。

Raw Planを改ざんしてRaw Plan identityを再計算しても、Contractからの独立再導出に一致しないため拒否する。

## 6. Frozen authority

CanonicalなFreeze入口は次だけである。

```cpp
FreezeVerifiedComposition(
    const ValidatedCompositionContract&,
    const VerifiedCompositionPlan&);
```

RawCompositionPlanを直接受け取るoverloadは存在しない。

High-level ReaderはR1 Container検証後に次を行う。

1. Contractをdecode
2. 埋込みLeafからEndpoint authorityを再導出
3. Contractを再検証
4. Raw decision bytesをdecode
5. Independent VerifyAndSealを再実行
6. 保存Certificateと再導出Certificateを完全比較

したがって、R1 low-level Writerでdigest-validな偽Artifactを組み立ててもR2 Readerは拒否する。

## 7. R2の非主張

R2は次をまだ証明しない。

- Frozen CompositionのD3D12実行
- Shared DeviceDomain
- Composition-owned native Buffer
- runtime state handoff
- runtime signal／wait
- repeated-frame fan-out retirement
- Composition Recoveryの実行

これらはR3以降で扱う。
