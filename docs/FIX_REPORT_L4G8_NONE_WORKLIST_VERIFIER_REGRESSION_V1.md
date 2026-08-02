# Level 4 Generalization 8 修正報告 — None Worklist Verifier Regression

## 症状

Windows Full GateのDebug／Release buildは成功したが、統合設計試験が次で失敗した。

```text
New SGE4統合設計試験に失敗しました：Invocationが検証または実行の契約に違反しています。
```

## 原因

Generalization 8で独立Dynamic Verifierへ追加したCompact Worklist集合照合が、Worklist契約を持つInvocationだけでなく、既存の`CompactWorklistModeV1::None` Invocationにも無条件で適用されていた。

```text
actual.compactWorklist.memberIndices == exact Transition set
```

`None`契約ではPlanner／Verifierが導出すべきCompact Worklistは空である。一方、通常のAuthority-only／Verified Dense Slot Invocationは非空Transition setを持ち得る。このため、G1～G7型InvocationがG8追加後に誤って拒否された。

## 修正

exact Transition setとのmember列一致を要求するのは、Frozen Contractが`VerifiedU32` Compact Worklistを要求する場合だけとした。

```text
CompactWorklistModeV1::VerifiedU32
  -> memberIndicesはexact Transition setと一致必須

CompactWorklistModeV1::None
  -> memberIndicesは空
  -> mode／target／maxCount／identityをExpected None Worklistと照合
```

`None`時の空列、無効target、zero max count、Canonical identityは既存の`SameCompactWorklist`照合で引き続き検証される。したがって、未契約Invocationへ任意member列を混入させることは受理されない。

## 回帰確認

Debug相当／Release相当のC++23厳格Harnessで次を同時確認した。

1. Authority-only、非空Transition `{0,2,7}`、Compact Worklist契約なしが独立Verifierを通過する。
2. Verified Compact Worklist `{1,4,7}`がCanonical順で独立Verifierを通過する。
3. `None` Decisionは空member列を維持する。
4. `VerifiedU32` Decisionはexact Transition setを維持する。

この修正はVerifierの適用条件だけを狭めるもので、SGE4UNI 2.8、SGE4INV 1.6、ABI bytes、Planner、Runtime物質化、Windows資格Fixtureを変更しない。
