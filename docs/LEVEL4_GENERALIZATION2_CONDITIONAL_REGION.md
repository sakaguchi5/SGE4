> Current status: Generalization 3により現行ProductionはSGE4UNI 2.3へ進んだ。本書の2.2記述はGeneralization 2完成時点の契約記録であり、Conditional Region／SGE4INV 1.3の意味は維持される。

# Level 4 Generalization 2 — Conditional Region

## 1. 目的

Generalization 2は、毎frameすべてのLeafを実行する静的Compositionを、Compilerが固定した有限の条件領域へ一般化する。

Runtimeへ自由な`if`を追加することは目的ではない。条件、True／False branch、所属Leaf、選択結果、実行Leaf集合をすべてFrozen authorityへ含める。

```text
Composition Dynamic Contract
  predicate + True Leaves + False Leaves
          ↓
Dynamic Planner
  exact setからbranchを提案
          ↓
Independent Dynamic Verifier
  exact setからbranchとenabled Leavesを再導出
          ↓
SGE4INV 1.3 Conditional Execution Section
          ↓
Runtime
  predicateを再評価せずenabled LeavesだけをSubmit
```

## 2. Frozen形式

- Frozen Composition: `SGE4UNI 2.2`
- Dynamic Contract: schema 3
- Frozen Dynamic Invocation: `SGE4INV 1.3`
- Invocation Manifest: schema 4
- Conditional Execution Section: schema 1

Composition ABI 2.2は2.1の平坦Section、Schema 17 Leaf bytes、Contract、Plan、Certificate、Authority Ledgerを維持し、Dynamic Contractだけをschema 3へ更新する。

## 3. Conditional Region契約

```text
ConditionalRegionV1
  id
  predicate
  trueLeaves[]
  falseLeaves[]
```

初期版は非ネストである。Region IDは0からdenseであり、Leaf ID集合は昇順・重複なしでなければならない。Leafは最大一つのRegion／branchにだけ所属できる。

対応predicateは、Dynamic Verifierがすでに独立再導出するexact setの非空判定だけである。

```text
ActiveSetNonEmpty
ActivationSetNonEmpty
DeactivationSetNonEmpty
UpdateSetNonEmpty
RetainSetNonEmpty
TransitionSetNonEmpty
```

任意のcaller bool、CPU callback、GPU query、時刻、性能測定結果をpredicateとして受理しない。

## 4. Graph closure

選択branchが単独で実行可能であることをComposition Toolchainで固定する。

- unconditional producerからconditional consumerへのFlowは許可する
- conditional producerからunconditional consumerへのFlowは拒否する
- conditional producerから異なるRegionまたは異なるbranchへのFlowは拒否する
- 同一Region／同一branch内のFlowは許可する
- Presenter LeafはConditionalにしない
- Composition OutputのproducerはConditionalでもよい

Conditional Composition Outputが未選択のframeでは、Resourceとcompletion tokenは直前に受理された状態を保持する。Runtimeがfallback値を生成したりResourceを暗黙Clearしたりしない。

## 5. Dynamic authority

Dynamic Plannerと独立Verifierは、それぞれ次を導出する。

```text
ConditionalRegionSelectionV1[]
  region id
  predicate value

enabledLeaves[]
  当該frameでSubmitされる完全なLeaf集合

ConditionalExecutionIdentity
  Composition leaf count
  selections
  enabledLeaves
```

Unconditional Leafは常にenabledである。各RegionではTrue／Falseの一方だけをenabledへ加える。False branchが空で、全LeafがそのRegionのTrue branchに属する場合、正しい結果はzero-Leaf submissionになり得る。

Runtimeはpredicateを再評価しない。Frozen selectionとComposition contractからenabled集合を機械的に照合し、Seal済みidentityと一致した場合だけD3D12 Runtimeへ渡す。

## 6. Verified Dynamic Executionとの接続

Conditional RegionはGeneralization 1のprivate dense shadowと独立ではない。

- Update／Clear transitionは、対象Leafが未選択でもprivate shadowへ適用する
- Native submissionが成功した場合だけshadowとHistoryをCommitする
- 対象Leafが未選択ならDynamic Slot bytesをGPUへ渡さない
- 後のframeで対象Leafが再選択された場合、Commit済みの完全なdense shadowを渡す

これにより、条件によるGPU実行省略と、exact Dynamic historyの進行を分離できる。

## 7. Runtime規則

D3D12 Composition Runtimeは`enabledLeaves`だけを静的schedule順でSubmitする。

- enabled Leaf集合はLeaf ID昇順・重複なし
- scheduleそのものはFrozen Planのまま変更しない
- 未選択LeafへDynamic Dataを渡すことは禁止
- zero-Leaf submissionを成功として扱う
- 未選択LeafのEndpoint、Resource state、completion tokenを更新しない
- RuntimeはPlanner／Verifierを呼ばない

Conditional Regionは「新しいscheduleをRuntimeで作る」能力ではなく、「Frozen scheduleからSeal済み部分集合だけを機械的に実行する」能力である。

## 8. Qualification

Architecture Gateは次を検証する。

- SGE4UNI 2.2 Dynamic Contractのround-trip
- True branch／False branchの独立導出
- enabled Leaf集合の改竄拒否
- 同一Leafの複数Region所属拒否
- branchを跨ぐResource Flow拒否
- SGE4INV 1.3 Conditional Execution Section

Windows Gateは次を実GPU／WARP経路で検証する。

```text
Active non-empty
  -> True branch
  -> 2 Leaf Submit
  -> GPU Readback更新

Active empty
  -> False branch(empty)
  -> zero-Leaf Submit
  -> Dynamic shadow／HistoryだけCommit
  -> Composition Outputは直前状態を保持

再びActive non-empty
  -> True branch
  -> Commit済みshadow全体をGPUへ反映

Controlled Recovery
  -> shadow／History失効
  -> RecoverySeed
  -> True branch再実行
  -> Readback再構築
```

## 9. 非目標

Generalization 2には次を含めない。

- nested Conditional Region
- arbitrary bool slot／callback predicate
- Conditional Presenter
- branchごとの異なるResource schema
- Runtimeによるcandidate選択
- Frozen Variant Set
- ExecuteIndirect／可変DispatchによるLeaf内部work量の省略
- Partial Recovery

次段階でこれらを必要とする場合も、具体的な意味、拒否条件、Recovery、観測結果が確定してから別Generalizationとして追加する。

## 10. zero-Leaf shadow Commit修正

初回Windows資格試験により、false branchのzero-Leaf submissionでverified Clear済みshadowがCommitされず、Historyだけが進む欠陥が検出された。

原因は、GPUへのDynamic Slot binding有無を表す`hasBinding`を、private shadowの受理条件にも使用していたことである。修正後は、Native submissionが成功し、Compositionが`VerifiedDenseSlot`契約なら、Leaf選択の有無に関係なくprepared shadowをHistoryと同時にCommitする。

この修正はConditional predicate、enabled Leaf集合、Frozen ABI、Planner／Verifier sealを変更しない。実行時受理状態だけをGeneralization 2の設計契約どおりに修正する。
