# Level 4 Generalization 8 — Verified Compact Sparse Worklist

## 1. 目的

Level 5垂直実験1では、exact Transition countを`DispatchIndirect X`へ接続することで、prefix Active集合に対して実GPU workを削減できた。しかしShaderはDispatch ordinalをそのままmember IDとして使用していた。

```text
ordinal 0 -> member 0
ordinal 1 -> member 1
...
```

したがって正しく実行できる疎集合は、

```text
{0, 1, ..., K-1}
```

に限定されていた。

Generalization 8は、exact Transition setからCanonicalなcompact member-index listをPlannerと独立Verifierが別々に導出し、Dispatch ordinalから実member IDへの写像をFrozen Dynamic authorityへ含める。

```text
Exact Transition Set
  -> canonical ascending uint32 member IDs
  -> Dynamic Planner proposal
  -> Independent Dynamic Verifier
  -> SGE4INV Compact Worklist Section
  -> fixed-size Dynamic Slot materialization
  -> DispatchIndirect X = index count
  -> Shader ordinal -> compactIndices[ordinal] -> member ID
```

Runtimeはindexを並べ替え、除外、追加、推測しない。

## 2. Frozen形式

Production Frozen Compositionは`SGE4UNI 2.8`である。

```text
SGE4UNI 2.8
  Manifest schema                 2
  Contract Data schema            3
  Verified Decision Data schema   3
  Dynamic Contract schema         6
  Leaf Package                    Schema 17
```

Frozen Dynamic Invocationは`SGE4INV 1.6`、Manifest schema 7である。既存7 Sectionに、必須かつexecution-affectingな次のSectionを追加する。

```text
Compact Worklist Section = kind 8 / schema 1
```

Leaf Package ABIは変更しない。対象Leafは既存Schema 17内に、dense payload用Dynamic Slotとcompact index list用Dynamic Slotを別々に持つ。

## 3. Composition Dynamic Contract

Verified Indirect Dispatch Contractへ次を追加する。

```text
CompactWorklistMode
  None
  VerifiedU32

targetIndexListDynamicSlot
```

初期版の`VerifiedU32`契約は次を要求する。

```text
Indirect mode              VerifiedDispatch
Target                     1 unconditional Compute Leaf
Target Compute Command     1 command
Index element              uint32
Index ordering             strictly ascending
Maximum index count        maxWorkCount = universeCount
Index-list Slot bytes      maxWorkCount * 4
Dense payload Slot         separate Dynamic Slot
Dispatch mapping           X = worklist count, Y = 1, Z = 1
```

Compact Worklist Slotを通常のdense execution routeと共有してはならない。Caller、Runtime、別routeによる二重authorityを禁止するためである。

ABI 1.1はCompact Worklistを表現しない。Migrationは`CompactWorklistMode=None`だけをschema 6へ明示変換し、旧bytesからindex listやtarget Slotを推測しない。

## 4. Canonical Worklist

Plannerと独立Verifierは、同じexact Transition setから次を導出する。

```text
memberIndices = TransitionSet.Indices()
```

`ExactIndexSetV1`が所有するCanonical順をそのまま使用する。

例:

```text
Transition set = {1, 4, 7}
Compact worklist = [1, 4, 7]
workCount = 3
Dispatch X = 3
```

Activation、Deactivation、Modified SurvivorはすべてTransition setへ含まれる。Deactivation memberについては、Generalization 1／6のprivate dense shadowが先にzero化され、Shaderがworklistでそのmemberを実行してGPU outputへzeroを書き戻す。

## 5. Frozen Dynamic Invocation

`VerifiedCompactWorklistV1`は次を保存する。

```text
mode
target Leaf
target Dynamic Slot
maxIndexCount
memberIndices[]
Compact Worklist identity
```

Identityはmode、route、maximum、count、全member IDをbindする。

Dynamic Decision、Verification Seal、Frozen Invocation identity、ManifestはCompact Worklist identityをbindする。Indirect Dispatch SectionのworkCountとCompact Worklistのindex数は完全一致しなければならない。

独立VerifierはProposal内のlistを信頼せず、requestのexact Transition setから再導出して比較する。

## 6. Runtime materialization

Runtime Sessionは受理済みworklistを、Compositionが固定したDynamic Slotへ次の形で物質化する。

```text
[index 0][index 1]...[index K-1][zero padding ...]
```

Slotの物理サイズは常に、

```text
maxWorkCount * sizeof(uint32)
```

である。実index数Kだけを先頭へlittle-endian uint32で書き、残りをzeroで埋める。

Runtimeが行うのは次だけである。

1. Composition routeとInvocation routeの一致を検査する。
2. Compact Worklist identityを再計算して照合する。
3. listがexact Transition setとbyte意味で一致することを照合する。
4. fixed-size SlotへCanonical順でcopyしzero paddingする。
5. Seal済みDispatch引数と同時に対象Leafへ渡す。

Runtimeはmembershipから別のworklistを構築せず、sort、deduplicate、clampを行わない。

## 7. Shader実行モデル

資格Leafは次の写像を行う。

```hlsl
StructuredBuffer<float4> DynamicValues : register(t0);
StructuredBuffer<uint> CompactIndices : register(t1);
RWStructuredBuffer<float4> Output : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    const uint member = CompactIndices[id.x];
    Output[member] = DynamicValues[member];
}
```

`Dispatch X`はindex数Kであり、Shader invocation ordinalは直接member IDではない。

## 8. Caller上書き禁止

Compact Worklist SlotはFrozen authorityが所有する。`FrameInput.leafDynamicData`が同じ`(Leaf, Slot)`を渡した場合、submission前に拒否する。

Dense payload routeとCompact Worklist routeは別のSlotであるが、どちらもCaller上書きを禁止する。

## 9. CommitとRecovery

Compact WorklistはInvocationごとの一時的execution dataであり、persistent dense shadowではない。

```text
persistent:
  Dynamic History
  route private shadows

ephemeral per Invocation:
  compact index list
  Dispatch arguments
```

native submissionが成功した場合だけ、従来どおりHistoryと全route shadowを原子的にCommitする。Compact Worklist自体を次frameへ履歴として保持しない。次frameでは新しいexact Transition setから新しいworklistをSealする。

Whole-composition Recoveryでは、旧Device epochのInvocationとworklist bindingを失効させる。RecoverySeedのexact Transition setから新しいworklistを再導出し、新Device epochへ物質化する。

## 10. 資格試験

Architecture Gate:

- `SGE4UNI 2.8`／Dynamic Contract schema 6
- `SGE4INV 1.6`／Manifest schema 7／Section kind 8
- exact set `{1,4,7}`からCanonical list `[1,4,7]`
- 非Canonical list `[1,7,4]`の独立Verifier拒否
- worklist identity改竄拒否
- fixed-size Slotとzero padding
- Compact Worklist Slotとdense routeの重複拒否
- ABI 1 migrationがCompact Worklistを推測しないこと

Windows Gate:

```text
InitialSeed {1,4,7}
  -> Dispatch X=3
  -> GPU outputの1,4,7だけ更新

ContinueHistory
  Active {0,3,7}
  Deactivate {1,4}
  Modified {7}
  -> worklist {0,1,3,4,7}
  -> 1,4へzeroを書戻し
  -> 0,3,7を更新

Caller collision
  -> index-list Slot上書きを拒否

Controlled Recovery
  -> old worklist／epoch失効
  -> RecoverySeed {2,6}
  -> GPU outputの2,6だけ再構築
```

## 11. 非目標

- GPUが生成するindex list
- prefix sum／compactionをGPU内で実行すること
- uint64 index
- duplicate indexを意味として許可すること
- 非Canonical順の保持
- routeごとの独立worklist
- 複数Indirect target
- Draw／DrawIndexed indirect
- worklistのTemporal保持
- Runtimeによるindex transformation
- Frozen Variant Set
- Runtime performance policy

Generalization 8の目的は一般的なGPU compaction frameworkを作ることではない。CPU側で既に独立検証されているexact sparse setを、member identityを失わず実GPU workへ接続する最小の完全経路を正本化することである。
