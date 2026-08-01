# Level 4 Generalization 4 — None Indirect Work Count検証修正

## 1. 検出

Windows統合設計試験で、Generalization 4以前から存在するAuthorityOnly CompositionのFrozen Dynamic Invocation生成が拒否された。

失敗したInvocationはexact Transition setを持つが、CompositionにはVerified Indirect Dispatch契約がない。

## 2. 原因

Dynamic Decisionの`indirectWorkCount`はGeneralization 4以前から、exact Transition setの要素数を表す。

Generalization 4で追加した`VerifiedIndirectDispatchV1::workCount`は、Compositionが`VerifiedDispatch`契約を持つ場合だけ、実GPUのDispatch Xを表す。

しかし独立Verifierが次をmodeに関係なく要求していた。

```text
indirectDispatch.workCount == indirectWorkCount
```

`IndirectExecutionModeV1::None`では正しい値は次である。

```text
indirectWorkCount             = exact Transition count
indirectDispatch.workCount    = 0
indirectDispatch.Dispatch X   = 0
```

したがって、既存AuthorityOnly／VerifiedDenseSlot Compositionのtransitionが1件以上あるInvocationが誤って拒否された。

## 3. 修正

work count一致条件を`VerifiedDispatch` modeに限定した。

```text
VerifiedDispatch
  dispatch.workCount == decision.indirectWorkCount

None
  dispatch.workCount == 0
  Dispatch X == 0
```

`None`成果物のzero引数、route absence、identityは、独立再導出されたexpected indirect dispatchとの完全一致およびRuntime Sessionの既存検証で維持する。

## 4. 回帰試験

既存AuthorityOnly Invocationについて次を同時に確認する。

```text
exact Transition count = 3
Indirect mode          = None
Dispatch workCount     = 0
Dispatch X             = 0
```

これにより、Dynamic algebra上のtransition量と、D3D12 indirect execution量が異なる責務であることを固定する。

## 5. 非変更範囲

- SGE4UNI 2.4
- SGE4INV 1.4
- Dynamic Contract schema 4
- exact Transition set
- VerifiedDispatch時のwork count一致
- D3D12 ExecuteIndirect
- Runtime Session route／identity検証
- Recovery
