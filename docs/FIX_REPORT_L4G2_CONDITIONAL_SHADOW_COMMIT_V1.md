# Fix Report — Level 4 Generalization 2 Conditional Shadow Commit

## 1. 対象

基準：`830b59efb252b2c988ab94a24ce99e4850eeee5c`へLevel 4 Generalization 2を適用した構成。

Windows統合実行資格試験で、次の失敗を確認した。

```text
再有効化時にCommit済みDynamic shadowがGPUへ反映されませんでした。
```

## 2. 原因

`Session::PrepareDynamicExecution`は、Conditional false branchで対象Leafが未選択でも、verified Clear／Updateを一時的なdense shadowへ正しく適用していた。

しかし`Session::CommitSubmission`は、次の条件でしかprivate shadowをCommitしていなかった。

```text
prepared.hasBinding == true
```

`hasBinding`が表すのは、当該frameで対象Leafが選択され、dense Dynamic Slot bytesをNative Runtimeへ渡したかどうかである。verified transitionをRuntime stateとして受理すべきかどうかではない。

そのためzero-Leaf false branchでは、

```text
History             Commitされる
verified Clear      Prepared shadowへ適用される
private shadow      Commitされない
```

という不整合が生じた。後の再有効化frameは新しいHistoryからtransitionを導出する一方、古いprivate shadowを基礎にしたため、無効化済みmemberのbytesが残った。

## 3. 修正

Native submission成功後、CompositionのDynamic execution modeが`VerifiedDenseSlot`なら、Leaf選択の有無に関係なくprepared dense shadowをHistoryと同時にCommitする。

```text
hasBinding
  GPUへDynamic Slot bindingを渡す条件

VerifiedDenseSlot
  prepared shadowを受理する条件
```

Authority-only Compositionはshadowを持たないため変更しない。

## 4. 維持される原則

- Native submission失敗時はHistoryもshadowもCommitしない。
- zero-Leaf submissionは正式な成功submissionである。
- 未選択LeafへDynamic Slot bytesを渡さない。
- 未選択frameでもexact Update／Clearはprivate shadowへ反映する。
- 再有効化時はCommit済みの完全なdense shadowを渡す。
- ABI、Frozen identity、Planner、Verifier、Composition Contractは変更しない。

## 5. 回帰試験

GPUを必要としないRuntime Session試験を追加した。

```text
InitialSeed
  member 1をUpdate
  true branch
  shadowをCommit

ContinueHistory
  Activeを空へ変更
  member 1をClear
  false branch
  zero-Leaf
  hasBinding = false
  Clear済みshadowをCommit

ContinueHistory
  member 3をActivate／Update
  true branchへ再有効化
  prepared dense shadowはmember 3だけを保持
```

これにより、`hasBinding`をshadow Commit条件へ再利用する退行をArchitecture Gateで検出する。
