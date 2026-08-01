# L4G4 AuthorityOnly Transition Audit 修正報告

## 症状

Windows資格試験で、exact transition count 3をSealしたInvocationのD3D12 Submit後に、
`Submission::verifiedTransitionCount`が3にならなかった。

## 原因

Generalization 4の資格CompositionはDynamic execution modeが`AuthorityOnly`である。
このmodeではtransitionはDynamic algebraおよびVerified indirect dispatchの入力として
使用されるが、Generalization 1のprivate dense shadowへは適用されない。

`Submission::verifiedTransitionCount`が
`PreparedDynamicExecutionV1::appliedTransitionCount`を報告していたため、

- exact verified transition count = 3
- GPU indirect work count = 3
- dense shadow applied transition count = 0

という正しい状態を、transition count 0として誤報告していた。

## 修正

`PreparedDynamicExecutionV1`へ`verifiedTransitionCount`を追加し、
独立VerifierがSealした`DynamicDecisionV1::indirectWorkCount`から設定する。

- `verifiedTransitionCount`: exact Transition set件数
- `appliedTransitionCount`: VerifiedDenseSlot shadowへ実際に適用した件数
- `indirectWorkCount`: GPU DispatchIndirectへ渡すwork件数

D3D12 RuntimeのSubmission監査値は`verifiedTransitionCount`を返す。
`VerifiedDenseSlot`では既存検査によりverified件数とapplied件数が一致する。
`AuthorityOnly + VerifiedDispatch`ではappliedは0のまま、verifiedとindirectは3になる。

Frozen ABI、Planner、Verifier、Dispatch引数、ExecuteIndirect実装は変更しない。
