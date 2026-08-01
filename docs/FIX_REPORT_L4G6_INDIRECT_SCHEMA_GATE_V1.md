# Level 4 Generalization 6 Fix 2 — Indirect Contract Schema Gate

## 発生した失敗

Windows統合設計試験は次のメッセージで停止した。

```text
Verified indirect dispatch契約がSGE4UNI 2.6へ固定されませんでした。
```

## 原因

Generalization 6はFrozen CompositionをSGE4UNI 2.6へ進め、Dynamic Contractをschema 5へ更新した。schema 5はGeneralization 4のVerified Indirect Dispatch routeを削除せず、Canonical multi-route tableと同じContract内に保持する。

しかし`60_UnifiedArchitectureTests`のIndirect回帰だけが、次の旧条件を残していた。

```cpp
DynamicContract().schemaVersion == 4
```

生成されたContractは正しくschema 5であるため、Indirect mode、target Leaf、target Compute Command、maxWorkCountがすべて正しくても、最初の条件だけでGateが失敗していた。

## 修正

G6 Multi-target試験とG4 Indirect回帰試験のschema照合を、数値literalではなくABI正本定数へ統一した。

```cpp
composition::artifact::FrozenCompositionAbi2DynamicContractSchema
```

これにより、SGE4UNI Production minor更新時にContract schemaを変更した場合も、旧literalの取り残しで保存済み能力を誤判定しない。

併せて、SGE4UNI 2.6をschema 4と記載していたauthority mapとacceptance matrixをschema 5へ修正した。

## 非変更範囲

- SGE4UNI 2.6 binary layout
- Dynamic Contract schema 5
- Verified Indirect Dispatch route encoding
- SGE4INV 1.5 Indirect Dispatch Section
- Dynamic Planner／独立Verifier
- D3D12 ExecuteIndirect
- Multi-target route／private shadow／Recovery

製品コードの変更はない。
