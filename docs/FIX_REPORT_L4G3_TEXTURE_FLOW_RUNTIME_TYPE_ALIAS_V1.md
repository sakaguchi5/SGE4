# Level 4 Generalization 3 修正記録 — Texture2D Flow Runtime型alias

## 現象

Windows／MSVC buildで、`CompositionSharedResources.h`の次のmember宣言が解釈できず、`texture2D`が不明なオーバーライド指定子として報告された。

```cpp
Texture2DFlowShape texture2D;
```

## 原因

`Texture2DFlowShape`の正式な所有者は`::sge4::composition`である。

D3D12 Runtimeは`RuntimeTypes.h`をComposition型との唯一の型境界として使用し、`LeafPackageId`、`ResourceFlowId`、`ResourceBoundary`などを`runtime_detail` namespaceへaliasしている。しかしGeneralization 3で追加した`Texture2DFlowShape`だけがこの一覧から欠落していた。

GCC／ClangによるPortable検査では、Windows D3D12 Runtime Translation Unit全体を実際のMSVC Header環境でcompileしていないため、この名前解決漏れを検出できなかった。

## 修正

`RuntimeTypes.h`へ正式aliasを追加した。

```cpp
using Texture2DFlowShape = model::Texture2DFlowShape;
```

`CompositionSharedResources.h`からComposition Headerを個別に追加includeしたり、型を複製したりしない。これによりD3D12 Runtimeが参照するComposition型は、引き続き`RuntimeTypes.h`へ集約される。

## 非変更範囲

次は変更していない。

- SGE4UNI 2.3
- Contract Data schema 2
- Verified Decision Data schema 2
- Leaf Schema 17
- SGE4INV 1.3
- Texture2D Flowの形状制約
- Planner／Verifier
- D3D12 Texture物質化、state handoff、readback
- Recovery契約

## 再確認

Windowsで`run_new_sge4_full_gate.bat`を再実行し、MSVC Debug／Release build後にHLSL、WARP Texture Flow、readback、Recovery、Actual Device removalまで確認する。
