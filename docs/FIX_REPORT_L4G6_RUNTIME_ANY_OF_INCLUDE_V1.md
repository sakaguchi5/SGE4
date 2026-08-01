# Level 4 Generalization 6 — Runtime `std::any_of` include修正

## 症状

Windows Debug buildで、`src/backends/d3d12/runtime/Runtime.cpp`の`std::any_of`に対して、MSVCが次を報告した。

```text
error C2039: 'any_of': 'std' のメンバーではありません
error C3861: 'any_of': 識別子が見つかりませんでした
```

## 原因

Generalization 6は、CallerがFrozen Composition所有のいずれかのDynamic route `(Leaf, Slot)`を上書きしていないか確認するため、D3D12 Runtime facadeへ`std::any_of`を追加した。

しかし同Translation Unitに標準Header `<algorithm>`を追加していなかった。Linux側の構文検査環境では他Headerからの間接includeによって見逃されたが、MSVCの正しいHeader境界では`std::any_of`が宣言されなかった。

## 修正

`Runtime.cpp`へ次を追加した。

```cpp
#include <algorithm>
```

Generalization 6のFrozen ABI、Dynamic route table、payload identity、private shadows、原子的Commit、Conditional binding、Recoveryの意味は変更しない。

## 再発防止境界

標準ライブラリ機能は間接includeへ依存せず、その宣言Headerを利用Translation Unitが直接includeする。今回の修正はcompile-time依存関係だけであり、Frozen bytesまたは実行結果を変更しない。
