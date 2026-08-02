> Current status: Generalization 8により現行ProductionはSGE4UNI 2.8／SGE4INV 1.6へ進んだ。本書は当該Generalization完成時点の契約記録であり、その意味は維持される。

# Level 4 Generalization 4 — Verified Indirect Work Execution

## 1. 目的

Generalization 1～3により、Dynamic Verifierはexact transition countを`indirectWorkCount`として確定し、Runtimeはverified payload、Conditional Leaf選択、Buffer／限定Texture2D Flowを実行できるようになった。しかしLeaf内部のCompute dispatch数はSchema 17 Compute Commandに固定されたままであり、verified countはGPU work量そのものを変更していなかった。

Generalization 4は、独立Verifierが確定したwork countをD3D12 `ExecuteIndirect(DISPATCH)`まで一つのauthorityで接続する。

```text
Exact membership algebra
  -> exact Transition set
  -> Dynamic Planner proposal
  -> Independent Dynamic Verifier
  -> Seal済みDispatch引数
  -> SGE4INV 1.4 Indirect Dispatch Section
  -> Runtime機械検証
  -> D3D12 ExecuteIndirect(DISPATCH)
```

RuntimeとExecutorはwork countを再計算、clamp、補完しない。

## 2. 限定範囲

初期能力は次だけを扱う。

```text
Target                    1 unconditional Compute Leaf
Compute Command           1 command
Indirect operation        Dispatch only
Maximum work count        Compositionで固定
Thread-group mapping      X = workCount, Y = 1, Z = 1
Shader thread group       Fixtureでは numthreads(1,1,1)
Count source              exact Transition set count
Argument command count    1
Device domain             single adapter / shared device domain
```

対象Compute CommandはSchema 17内で`X = maxWorkCount, Y = 1, Z = 1, flags = 0`として存在し、Frame Operation内でちょうど一度`ExecuteCompute`されなければならない。Leaf Package Schema 17は変更しない。

## 3. Frozen Composition ABI

Production Frozen Compositionは`SGE4UNI 2.4`、Dynamic Contract schema 4とする。

Dynamic Contractは従来のauthority／dense route／Conditional Regionに加え、次を固定する。

```text
IndirectExecutionMode
  None
  VerifiedDispatch

targetLeaf
targetComputeCommand
maxWorkCount
```

`VerifiedDispatch`では次を要求する。

- target Leafは存在する
- target LeafはConditional Regionに所属しない
- target Compute Commandは存在する
- `maxWorkCount == universeCount`
- static CommandのXがmaxWorkCountと一致する
- static CommandのY／Zは1
- flagsは0
- Frame Operation内の対象ExecuteComputeは一つだけ

ABI 1.1はIndirect契約を表現しない。Migrationは`IndirectExecutionMode=None`だけをschema 4へ明示変換し、旧bytesからIndirect targetを推測しない。

## 4. Frozen Dynamic Invocation ABI

Production Frozen Dynamic Invocationは`SGE4INV 1.4`、Manifest schema 5とする。必須Sectionは7個である。

```text
Manifest
Exact Sets
Transition Records
Next History
Execution Payload
Conditional Execution
Indirect Dispatch
```

Indirect Dispatch Sectionは次を保存する。

```text
mode
target Leaf
target Compute Command
maxWorkCount
workCount
threadGroupCountX
threadGroupCountY
threadGroupCountZ
Indirect Dispatch identity
```

Plannerと独立Verifierは、それぞれexact Transition setから次を導出する。

```text
workCount = |TransitionSet|
X = workCount
Y = 1
Z = 1
```

VerifierはProposal内のwork count、X／Y／Z、route、max、identityを独立再導出値と完全比較する。Dynamic Decision、Verification Seal、Next History、Frozen Invocation identityはIndirect Dispatch identityをbindする。

## 5. Runtime Session

Runtime Sessionは次だけを行う。

1. Frozen Composition routeとInvocation routeが一致するか検査する。
2. Indirect Dispatch identityを再計算してSeal済みidentityと比較する。
3. `workCount == decision.indirectWorkCount`を検査する。
4. `X == workCount`、`Y == 1`、`Z == 1`、`workCount <= maxWorkCount`を検査する。
5. target LeafがSeal済みenabled Leaf集合に含まれることを検査する。
6. Seal済み引数をnative Runtimeへ渡す。

Runtime Sessionはmembershipからwork countを再導出しない。対象LeafをConditional化しない初期契約により、zero-work frameでもtarget LeafはSubmitされ、Leaf内部のGPU workだけが0になる。

## 6. D3D12 Executor

Executorは初回利用時に`D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH`だけを持つCommand Signatureを作る。各frame slotは12-byteの`D3D12_DISPATCH_ARGUMENTS` Upload Bufferを所有する。

Submission前にExecutorは、Leaf Invocationに含まれるSeal済み値をargument bufferへ書く。対象`ExecuteCompute` operationだけを次へ置換する。

```text
Direct path:
  Dispatch(staticX, 1, 1)

Verified indirect path:
  ExecuteIndirect(dispatchSignature, 1, argumentBuffer, 0, nullptr, 0)
```

対象外のCompute Commandは従来どおり固定Dispatchする。対象Commandが実行されなかった、二度実行された、上限を超えた、routeが異なる場合はsubmissionを拒否する。

`workCount = 0`でも一つのIndirect commandを発行し、Dispatch引数X=0によりGPU shader workを発生させない。RuntimeがLeaf自体を省略する意味とは区別する。

## 7. Recovery

Whole-composition Recoveryでは、Command Signatureとframe-slot Indirect Argument Bufferを破棄し、新Device epochで再作成する。RecoverySeedのexact Transition countから新しいSeal済みDispatch引数を生成し、再実行する。

Old epochのInvocation、History、Resource handleは従来どおり拒否する。

## 8. 資格試験

Architecture Gate:

- `SGE4UNI 2.4`／Dynamic Contract schema 4
- `SGE4INV 1.4`／Manifest schema 5／Indirect Dispatch Section
- exact transition countからXを導出
- zero-work dispatch
- Dispatch引数改竄の独立Verifier拒否
- Runtime Sessionによるroute／identity／enabled Leaf検証
- ABI 1.1 authority-only migrationと直接生成ABI 2.4のbyte一致
- Generalization 1～3の既存契約回帰

Windows Gate:

```text
InitialSeed active empty
  -> workCount 0
  -> ExecuteIndirect X=0
  -> GPU outputはzeroのまま

ContinueHistory active {0,2,7}
  -> Transition count 3
  -> ExecuteIndirect X=3
  -> GPU観測は先頭3 workだけ更新

Retain-only frame
  -> workCount 0
  -> 既存GPU outputを保持

Controlled Recovery
  -> Command Signature／argument buffer再物質化
  -> RecoverySeed workCount 2
  -> GPU観測再構築
```

## 9. 意図的な非範囲

Generalization 4は次を行わない。

- 一般的なExecuteIndirect argument layout
- Draw／DrawIndexed indirect
- 複数Indirect target Leaf／Command
- Conditional target Leaf
- Y／Zの可変化
- thread-group sizeを用いたceil division
- GPUが生成するcount buffer
- count bufferから別Leafへのindirect chain
- Dynamic payloadの複数Leaf scatter
- Frozen Variant Set
- Temporal Flow
- Partial Recovery

この段階の目的はIndirect機能数を増やすことではなく、すでに検証済みのexact Dynamic quantityを実GPU work量まで権威切れなく貫通させることである。

## 11. None契約とDynamic indirectWorkCountの分離

初回Windows統合設計試験により、Indirect契約を持たない既存Invocationが独立Verifierに拒否される欠陥を検出した。

Dynamic Decisionの`indirectWorkCount`は従来からexact Transition countを表す。一方、`VerifiedIndirectDispatchV1::workCount`は`VerifiedDispatch`契約が存在する場合だけGPU Dispatch量を表す。

したがって正しい関係は次である。

```text
VerifiedDispatch:
  dispatch workCount == decision indirectWorkCount

None:
  decision indirectWorkCount = exact Transition count
  dispatch workCount = 0
```

修正後の独立Verifierはmodeごとにこの関係を検証する。Runtime Sessionは`None`成果物にrouteまたは非zero Dispatch引数が含まれないことを引き続き拒否する。

## AuthorityOnly transition監査値の修正

初回Windows資格試験では、`AuthorityOnly + VerifiedDispatch` Compositionで
exact transition count 3とDispatchIndirect work count 3が成立していた一方、
`Submission::verifiedTransitionCount`が0と報告された。

原因は、監査値がprivate dense shadowへ実際に適用した件数を参照していたためである。
AuthorityOnlyはshadowを持たないため、次の三量を明示的に分離する。

```text
verifiedTransitionCount   = exact Transition set件数
appliedTransitionCount    = dense shadowへ適用した件数
verifiedIndirectWorkCount = DispatchIndirect work件数
```

AuthorityOnly + VerifiedDispatchでは`3 / 0 / 3`が正しい。
VerifiedDenseSlotでは既存のpayload全対応検査によりverifiedとappliedが一致する。
