> Current status: Generalization 7により現行ProductionはSGE4UNI 2.7／SGE4INV 1.5へ進んだ。本書の2.1／1.2記述はGeneralization 1完成時点の契約記録である。

# Level 4 Generalization 1 — Verified Dynamic Execution

## 1. 目的

SGE4 v2.0では、Dynamic Planner／VerifierがActive、Activation、Deactivation、Update、Retain、Transition、exact write set、verified indirect quantityを確定していた。しかしRuntimeはそのFrozen InvocationをHistory authorityとして検証した後、Callerが別に渡したLeaf dynamic bytesで静的Compositionを実行していた。

Generalization 1は、この二つの事実源を統合する。

```text
Composition Dynamic Contract
  target Leaf / target Dynamic Slot / memberBytes / universe
        ↓
Dynamic Planner + Independent Verifier
  exact Update set / Clear set / Transition records
  exact Update payload / payload identity
        ↓
SGE4INV 1.2
        ↓
Runtime private dense shadow
  Update = verified payloadで置換
  Clear  = zero化
  Retain = 変更しない
        ↓
Leaf Dynamic Slot bytes
        ↓
D3D12 submission
```

RuntimeはMembershipや実行routeを再判断しない。

## 2. Frozen Composition ABI

Production Frozen Compositionは`SGE4UNI 2.1`とする。Dynamic Contract schema 2は次を固定する。

- `universeCount`
- `executionMode`
- `targetLeaf`
- `targetDynamicSlot`
- `memberBytes`

`AuthorityOnly`ではrouteを持たない。`VerifiedDenseSlot`では、一つのmember universeを一つのLeaf Dynamic Slotへdenseに写像する。

```text
slot requiredBytes == universeCount * memberBytes
```

この一致はComposition freezeとreadの双方で、埋め込まれたSchema 17 Leaf Packageをdecodeして検証する。

## 3. Frozen Dynamic Invocation ABI

Production Frozen Dynamic Invocationは`SGE4INV 1.2`、Manifest schema 3とする。新しい必須Execution Payload Sectionは次を所有する。

- execution mode
- target Leaf
- target Dynamic Slot
- member byte幅
- canonical member順のUpdate payload
- payload identity

Update payloadのmember集合は、Planner／Verifierが導出したexact Update setと完全一致しなければならない。ActivationとModified Survivorにはpayloadが必須であり、DeactivationはClear recordだけを持つ。Retain memberへpayloadを渡してはならない。

## 4. Runtime commit規則

Runtime SessionはComposition load時にzero初期化されたdense shadowを所有する。

Submission前:

1. Composition routeとInvocation routeの一致を検証する。
2. payload identityを再計算する。
3. transition recordをcanonical member順で適用した候補shadowを作る。
4. Update recordには同一memberのpayloadを要求する。
5. Clear recordは該当rangeをzero化する。
6. `appliedTransitionCount == verified indirectWorkCount`を要求する。

D3D12 submission成功後だけ:

```text
accepted History = Invocation NextHistory
accepted shadow  = prepared shadow
```

を同時にCommitする。native submissionが失敗した場合、Historyもshadowも進めない。

## 5. Caller上書き禁止

Verified routeが所有するLeaf／Dynamic Slotと同じbindingを`FrameInput.leafDynamicData`から渡した場合、Runtimeはsubmission前に拒否する。これによりFrozen InvocationとCaller bytesの二重authorityを作らない。

AuthorityOnly Compositionでは、従来の明示的FrameInput dynamic dataを引き続き利用できる。

## 6. Recovery

whole-composition Recoveryでは、Runtime Historyとdense shadowを同時に失効させ、shadowをzeroへ戻す。External rebind acknowledgement後の`RecoverySeed`はActive全memberのpayloadを要求し、GPU-visible slotを完全再構築する。

## 7. 資格試験

Architecture試験:

- SGE4UNI 2.1／Dynamic Contract schema 2
- SGE4INV 1.2／Execution Payload Section
- payload identity
- exact Update setとpayload setの一致
- payload欠落拒否
- ABI 1.1 authority-only migrationから直接生成SGE4UNI 2.1へのbyte一致

Windows資格試験:

- InitialSeedのUpdateがGPU readbackへ現れる
- ContinueHistoryのActivation／Modified／DeactivationがUpdate／Retain／Clearとして現れる
- transition 0のRetain-only frameでbytesが維持される
- Callerによるverified slot上書き拒否
- payload欠落拒否
- Controlled Recovery後のRecoverySeed再構築

## 8. 意図的な非範囲

Generalization 1は次を行わない。

- ExecuteIndirect／DispatchIndirect
- transition数に応じたLeaf dispatch自体の省略
- Conditional Region
- 複数Leaf／複数Dynamic Slotへのscatter
- Texture Flow
- Frozen Variant Set
- Partial Recovery

したがって`verified indirectWorkCount`は、今回actualに適用したtransition record数としてRuntime報告へ接続されるが、GPU dispatch countそのものを可変化するものではない。
