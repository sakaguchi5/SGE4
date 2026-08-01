# Level 4 Generalization 6 — Multi-target Verified Dynamic Routing

## 1. 目的

Generalization 1は、一つのDynamic universeを一つのLeaf Dynamic Slotへdenseに写像した。Generalization 6は、その単一route制限を、同じCanonical member payloadから複数Leaf／複数Dynamic Slotへ固定byte sliceを配布できる形へ一般化する。

```text
Canonical member payload
  [Transform bytes][Bounds bytes][Material bytes]
          |               |              |
          v               v              v
  Leaf A / Slot 0   Leaf B / Slot 1  Leaf C / Slot 0
```

Runtimeは型変換、再計算、member選別を行わない。CompositionがFreezeしたsource byte sliceを、独立Verifierが再検証し、Frozen Dynamic InvocationがSealしたCanonical payloadから機械的に各private shadowへcopyする。

## 2. Frozen形式

Production Frozen Compositionは`SGE4UNI 2.6`である。

```text
SGE4UNI 2.6
  Manifest schema 2
  Leaf Table schema 1
  complete Schema 17 Leaf Package bytes
  Contract Data schema 2
  Verified Decision Data schema 2
  Verification Certificate schema 1
  Authority Ledger schema 2
  Dynamic Contract schema 5
```

Production Frozen Dynamic Invocationは`SGE4INV 1.5`、Manifest schema 6である。Execution Payload Sectionはschema 2へ進み、Canonical member byte幅、Canonical route table、exact Update payload、payload identityを保存する。

Leaf Package ABIはSchema 17を維持する。

## 3. Dynamic Contract schema 5

Verified route契約は次を所有する。

```text
canonicalMemberBytes
executionRoutes[]
  targetLeaf
  targetDynamicSlot
  sourceByteOffset
  routeMemberBytes
```

routeは`(targetLeaf, targetDynamicSlot)`の昇順でCanonical化し、同じtargetを二度所有してはならない。

各routeは次を満たす。

```text
routeMemberBytes > 0
sourceByteOffset + routeMemberBytes <= canonicalMemberBytes
target slot requiredBytes == universeCount * routeMemberBytes
```

source sliceの重なりは許可する。同じCanonical factを複数Leafへ複製するためである。Runtimeが重なりを解釈したり、所有関係を推測したりしない。

AuthorityOnly modeでは`canonicalMemberBytes == 0`かつroute tableは空である。

## 4. Dynamic Plannerと独立Verifier

Dynamic Plannerと独立Verifierは、それぞれ次を検証する。

- route tableがCanonical順で重複しない
- 全target Leaf／Slotが範囲内である
- 全source sliceがCanonical payload内である
- Update payload member集合がexact Update setと完全一致する
- 各Update payload byte数が`canonicalMemberBytes`と一致する
- payload identityがmode、Canonical byte幅、全route、全Update payloadをbindする

全routeは同じexact Active／Activation／Deactivation／Update／Retain／Transition集合を共有する。routeごとに別membershipを指定する入力は存在しない。

## 5. Runtime private shadows

Runtime Sessionはrouteごとに一つのprivate dense shadowを所有する。

```text
shadow[route].size
  = universeCount * routeMemberBytes
```

Submission前に全shadowの候補copyを作り、exact Transitionをmember単位で一度だけ走査する。

```text
Update:
  Canonical payload[source offset : source offset + route bytes]
  を全routeの同一member位置へcopy

Clear:
  全routeの同一member位置をzero化

Retain:
  全routeを変更しない
```

`appliedTransitionCount`はroute数倍にはしない。一つのverified Transitionを全routeへ原子的に反映した件数である。

Native submission成功後だけ、次を同時にCommitする。

```text
accepted History
all route shadows
```

一つのrouteだけが新しく、別routeが古い状態は受理しない。Native submission失敗時はHistoryも全shadowも進めない。

## 6. D3D12 Runtime

D3D12 Runtime facadeは、当該frameでenabledなrouteごとにLeaf Dynamic Data bindingを生成する。

CallerがFrozen contract所有のいずれかの`(Leaf, Slot)`へ手動bindingを渡した場合、対象LeafがConditionalで未選択でもsubmission前に拒否する。所有権はframeごとの選択ではなくFrozen Compositionが決めるためである。

未選択LeafのrouteはGPUへbindしないが、そのroute shadowにはverified Update／Clearを適用し、native submission成功後にCommitする。後のframeでLeafが再選択された場合、Commit済みの完全なshadowを渡す。

## 7. Recovery

Whole-composition RecoveryではHistoryと全route shadowを同時に失効させ、全shadowをzeroへ戻す。

External rebind acknowledgement後のRecoverySeedは、全Active memberについて完全なCanonical payloadを要求する。Runtimeは同じFrozen route tableを使用し、全route shadowを一つのRecovery submissionで再構築する。

## 8. 資格試験

Architecture Gate:

- SGE4UNI 2.6／Dynamic Contract schema 5
- SGE4INV 1.5／Manifest schema 6／Execution Payload schema 2
- route tableのRound-tripとidentity bind
- route順序改竄、duplicate target、slice範囲外、payload幅不一致の拒否
- 二つのroute shadowへ異なるCanonical sliceが写像されること
- Historyと全shadowの原子的Commit
- ABI 1.1 authority-only migrationと直接生成SGE4UNI 2.6のbyte一致

Windows WARP Gate:

```text
Canonical payload = [float4 A][float4 B]
  route 0 -> Leaf A Dynamic Slot
  route 1 -> Leaf B Dynamic Slot

InitialSeed
  -> 両LeafのGPU readbackへ別sliceが反映

ContinueHistory
  -> Update／Activation／Deactivation Clearが両routeへ同時反映

Caller collision
  -> どちらのFrozen-owned Slotでも拒否

Controlled Recovery
  -> 全route shadow失効
  -> RecoverySeed
  -> 両GPU outputを同時再構築
```

## 9. 意図的な非範囲

Generalization 6は次を含まない。

- routeごとの独立membership／Update set
- Runtimeでの型変換、行列計算、圧縮／展開
- 可変長member payload
- source gather、複数slice結合、destination offset
- GPU生成scatter／indirect routing
- 複数Dynamic universeの結合
- route単位の部分Commit／部分Recovery
- Temporal Flow
- Frozen Variant Set

目的はDynamic DSLを作ることではなく、一つのverified Canonical事実を複数Leafの実行入力へauthority切れなく配布することである。

## 10. Windows buildで検出した`std::any_of` Header修正

初回Windows buildで、D3D12 Runtime facadeがCaller bindingとFrozen-owned routeの衝突を検査するために使用する`std::any_of`について、`<algorithm>`の直接includeが欠落していることが検出された。

`Runtime.cpp`へ`#include <algorithm>`を追加した。これは標準ライブラリ宣言のcompile-time依存修正であり、SGE4UNI 2.6、SGE4INV 1.5、route authority、payload identity、shadow Commit、Recoveryの意味は変更しない。

## Windows統合設計試験で検出したIndirect schema Gate修正

Generalization 6初回Windows統合設計試験では、Verified Indirect Dispatch契約の回帰確認だけが旧Dynamic Contract schema 4を要求していたため、正しいschema 5成果物を誤って拒否した。

Verified Indirect routeはSGE4UNI 2.6 Dynamic Contract schema 5へ保持されており、製品のFreeze／Reader／Planner／Verifierには欠落がなかった。試験をABI正本定数`FrozenCompositionAbi2DynamicContractSchema`との照合へ変更し、Multi-target試験も同じ定数へ統一した。

この修正はFrozen bytes、Indirect route、Dispatch引数、Runtime、Executorを変更しない。
