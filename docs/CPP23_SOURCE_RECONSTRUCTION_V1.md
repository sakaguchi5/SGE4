# C++23 Source Reconstruction v1.5

> Historical baseline: 本文のABI 1.x記述は、Frozen Composition ABI 2.0の変更不能Oracleである`NewSGE4 v1.5.2`を説明する。現行Production Composition ABIは2.0である。

## 目的

v1.4で成立した二段Compiler、階層Frozen Artifact、Unified Runtime、D3D12 Compiler／Executor境界を維持したまま、Active sourceをC++23の標準的な失敗表現と型安全なCanonical encodingへ移行する。

今回の変更はSource reconstructionであり、Frozen ABIの更新ではない。次の形式、数値、Section順序、Digest入力は変更しない。

```text
Schema 17 Frozen Leaf Package
SGE4CMP format 1.0
SGE4UNI format 1.1
SGE4INV format 1.1
```

`/std:c++latest`は維持する。使用する言語・標準Library機能は、C++23として採用すると決めた範囲へ限定する。

## C++23移行

### std::expected

独自`Result<T, E>`と`Result<void, E>`を廃止し、すべてのActive product／qualification sourceを`std::expected<T, E>`へ移行した。

```text
旧:
Result<T, E>
Value()
Error()

新:
Expected<T, E> = std::expected<T, E>
value()
error()
std::unexpected<E>
```

SGE固有のError型、Error code、stage keyは維持する。標準化したのは成功値と失敗値の運搬方法であり、診断意味を一般的な文字列へ縮退させていない。

### Canonical encoding

`BinaryWriter`へ次を追加した。

```text
WriteEnumU16
WriteEnumU32
WriteCountU32
```

Enumのunderlying valueは`std::to_underlying`を経由する。`size_t`からABI上の`uint32_t`件数へ変換するときは、範囲検査を通過しない値を拒否する。

有効な入力について出力されるField幅、byte順序、Padding、Section順序はv1.4と同一である。

### compile-time Schema検証

`SchemaValidation.h`へ次の`consteval`検証を追加した。

```text
ValuesAreUnique
ValuesAreStrictlyIncreasing
AllValuesSatisfy
```

次の表を`static_assert`で検査する。

- Frozen Leaf Package Section kinds
- SGE4CMP Section kinds
- SGE4UNI Section kinds
- SGE4INV Section kinds
- D3D12 Operation Contract table

Schema定義に重複、Canonical順序違反、無効なVersionが入った場合は、実行試験より前にコンパイルを拒否する。

### 限定的なRanges利用

Canonical sort、固定表検索、除去など、意図が直接明確になる箇所だけに`std::ranges`と`std::erase_if`を使用した。

PlannerとIndependent Verifierの判断導出を共通Ranges pipelineへ統合していない。判定ロジックの独立性をSource上でも維持する。

## 採用していない機能

今回、次は採用していない。

```text
C++ Modules
std::flat_map / std::flat_setへの一括置換
std::mdspan
std::print
std::unreachable
[[assume]]
複雑なstd::views pipeline
```

外部bytesと不正入力を扱うReader／Verifierでは、到達不能の仮定ではなく明示的な拒否を維持する。

## 構造リファクタリング

巨大なD3D12実装を、同じTranslation Unit内の責務別実装断片へ分割した。外部Symbol、Static Library境界、ABI、Anonymous namespaceの可視範囲は維持する。

### D3D12 Executor

```text
Executor.cpp
  detail/ExecutorDiagnostics.inl
  detail/ExecutorDeviceDomain.inl
  detail/ExecutorExternalResources.inl
  detail/ExecutorInstance.inl
  detail/ExecutorApi.inl
```

### D3D12 Artifact encoding

```text
D3D12Encoding.cpp
  detail/EncodingSchemaAndTables.inl
  detail/EncodingRecordDecoders.inl
  detail/EncodingValidation.inl
  detail/EncodingPackageCodec.inl
  detail/EncodingOperationPayloads.inl
```

### D3D12 Compiler lowering

```text
D3D12PackageLowering.cpp
  detail/ShaderCompilation.inl
  detail/LoweringUtilities.inl
  detail/LoweringStages.inl
  detail/PackageLowering.inl
  detail/PackageFreezing.inl
  detail/CompilerApi.inl
```

この分割は、Device、Resource物質化、外部Resource、Command実行、Schema codec、Shader compile、Lowering、Freezeの責務をSource上で判別できるようにする。ExecutorがQueue、State、Allocation、Scheduleを新たに判断する能力は追加していない。

## 日本語Diagnostic

人間向けの成功表示、失敗表示、例外説明、Qualificationログ、bat表示を日本語化した。

次は機械契約であるため変更しない。

```text
Error code
stage key
Magic
Section name
Manifest key
Canonical identifier
D3D12 / DXGI API名
```

SourceとログはUTF-8を正本とし、全Projectへ`/utf-8`を適用する。外部HLSL CompilerやD3D12 Debug Layerが返す原文は、API由来の補足情報として保持され得る。

## ABI 1.x Golden bytes

v1.4 sourceとv1.5 sourceから同一入力を与え、次の出力がbyte単位で一致することをPortable環境で確認した。

```text
Sectioned Artifact        278 bytes
Frozen Leaf Package A     370 bytes
Frozen Leaf Package B     370 bytes
Frozen Composition       1811 bytes
```

Golden SHA-256:

```text
Sectioned Artifact
3f3c1a30b5daeb71eeeeaa6783805679d74da29e00802000de429f32033fc106

Frozen Leaf Package A
edbde3d1929b8f6208765761673b3fa265f48f3a936752adac3cd16a11d5f01e

Frozen Leaf Package B
2b4df7947fd216ba564ceb0c656d2ba2912baba0745226d1000d7e024e437932

Frozen Composition
46b140c4dfdf224be571fcc5f03ddf4f0489994e0b2a89aa0ac7466d919adfea
```

この検査は`60_UnifiedArchitectureTests`へ組み込み、Windows Full Gateでも毎回実行する。

完全な`SGE4UNI`と`SGE4INV`については、既存のDebug A／Debug B／Release byte一致Gateを維持する。Windowsでのv1.4との保存済み成果物比較を行う場合もReader／Writer形式は同じmajor／minorを使用する。

## 非変更事項

- Planner／Independent VerifierのProject境界
- RuntimeがFrozen成果物だけを消費する規則
- BackendがCompiler判断を再発見しない規則
- 40 carried invariants
- Recovery状態機械
- Device epoch規則
- External rebind gate
- Frozen ABI version
- Enum数値とSection kind
- Digest対象とCanonical順序

## 合格条件

```text
Static audit
Portable C++23 strict syntax
ABI 1.x Golden bytes
40 invariant Migration Acceptance
MSVC Debug / Release build
Debug A / Debug B / Release byte一致
WARP qualification
Controlled whole-composition recovery
Actual Device removal
MSBuild node正常終了
```

Windows固有項目は`run_new_sge4_full_gate.bat`で確定する。
