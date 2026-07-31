# Level 4 Generalization 1 Verified Dynamic Composition 修正報告 v1

## 1. 症状

`run_new_sge4_full_gate.bat` でDebug／Release buildは成功するが、統合設計試験が次で停止した。

```text
Verified Dynamic Compositionの生成に失敗しました。
```

## 2. 原因

製品側のComposition契約は、Level 4のLeaf合成を表すため、2個以上のLeafを要求する。

```text
ContractBuildInput.leaves.size() >= 2
```

最初のGeneralization 1資格Fixtureは、Verified Dynamic Slotを持つ実行Leafだけを1個登録していた。このため、製品契約は`contract/leaves`で正しく拒否していた。

さらに、その拒否を解消した後にD3D12 Loadで問題になる潜在不備として、Dynamic StructuredBufferの`strideBytes`が0のまま生成されていた。ExecutorはStructured SRVのstrideを必須とするため、member幅16 bytesを明示する必要があった。

## 3. 修正

Verified Dynamic資格Compositionを次の2 Leaf DAGへ変更した。

```text
Verified Dynamic Executor Leaf
    Dynamic Slotからdense float4配列を読む
    ↓ internal Buffer Flow
Dynamic Observation Leaf
    dense float4配列を全要素copyする
    ↓ Composition Output
GPU Readback
```

同時に次を修正した。

- Dynamic Buffer Fixtureへ`strideBytes`を明示する
- Dynamic実行対象Leaf IDを authored order の`0`と仮定しない
- stable keyのCanonical順から対象Leaf IDを導出する
- Caller上書き拒否試験もFrozen Compositionの実target Leaf／Slotを使用する
- Composition生成失敗時に内部stageとmessageを表示する

## 4. 変更していないもの

次は変更していない。

- SGE4UNI 2.1 ABI
- SGE4INV 1.2 ABI
- Dynamic Planner／Verifier
- Runtime dense shadowのUpdate／Clear／Retain規則
- Compositionの2 Leaf以上という既存不変条件
- Frozen identity／digest規則

今回の修正は、資格Fixtureを既存の製品契約へ正しく適合させるものであり、製品ABIの意味変更ではない。
