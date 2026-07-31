# Validation Report — Frozen Composition ABI 2.0

## Baseline evidence

`NewSGE4 v1.5.2`はユーザーのWindows環境で次を含むFull Gateを通過した変更不能Oracleである。

- MSVC Debug／Release build
- C++23 Source reconstruction
- ABI 1.x Golden bytes
- Architecture／Migration
- WARP qualification
- Controlled whole-Composition Recovery
- Actual Device removal
- MSBuild正常終了

ABI 2.0はこのBaselineのComposition意味、Schema 17 Leaf bytes、Planner／Verifier境界、Dynamic Invocation ABI、Runtime観測結果、Recovery意味を維持し、Frozen Composition containerだけを平坦化する。

## ABI変更

```text
変更前
  SGE4UNI 1.1
    CompleteComposition
      SGE4CMP 1.0
        Schema 17 Leaf bytes
        Contract
        Verified Decision
        Verification Certificate
    Authority Ledger
    Dynamic Contract

変更後
  SGE4UNI 2.0
    Manifest
    Leaf Table
    Schema 17 Leaf bytes
    Contract Data
    Verified Decision Data
    Verification Certificate
    Authority Ledger
    Dynamic Contract
```

維持:

```text
Frozen Leaf Target Schema: 17
Frozen Leaf Minimum Runtime: 17
Frozen Dynamic Invocation: SGE4INV 1.1
Project boundary: 15 Product + 3 Qualification
Carried invariants: 40
```

## Source変更

- `src/composition/artifact/abi2/FrozenCompositionAbi2.*`を追加
- Production `VerifiedCompositionArtifact`をABI 2.0専用Readerへ変更
- `CompositionToolchain`を平坦な8 Section Writer／Readerへ変更
- Composition Certificate schema 2を導入
- Composition Core digestを導入
- Runtime／D3D12 Runtimeへ外側SGE4UNI 2.0 bytesを渡すよう変更
- ABI 1.1／SGE4CMP Reader／Writerを`src/composition/migration/abi1/`へ隔離
- Plannerを再実行しない明示的v1.1 -> v2.0 Migratorを追加
- ABI 2.0 Portable Round-trip／Migration self-testを追加
- ABI 2.0 corruption corpusを追加
- Generic Sectioned Artifact Readerにzero-padding検証を追加
- Schema 17 Package Readerのzero-size Section overlap判定を修正

## この環境で実施した検証

### Static audit

```text
Project数: 18
Product Project数: 15
Active Product Translation Unit数: 47
Carried invariant数: 40
ProjectReference DAG: 非循環
```

追加監査:

- Production ABIは`SGE4UNI 2.0`に固定
- exact 8 direct Section
- Productionに`CompleteComposition`なし
- Production artifact treeにlegacy SGE4CMP containerなし
- legacy codeは`migration/abi1`だけに存在
- Production Readerはlegacy Reader／Writerを参照しない
- Schema 17／Runtime 17を維持
- SGE4INV 1.1を維持
- ABI 2.0 corruption／portable self-testが存在

### Portable C++23 strict syntax

Windows専用の次の3 Translation Unitを除き、Product 45本、Test／Fixture 6本、合計51本を検査した。

```text
-std=c++23
-Wall
-Wextra
-Wpedantic
-Werror
```

Windows専用:

```text
src/backends/d3d12/compiler/lowering/D3D12PackageLowering.cpp
src/backends/d3d12/executor/Executor.cpp
tests/61_UnifiedWindowsQualification/main.cpp
```

結果:

```text
strict syntax passed: 51
```

### ABI 1.x Oracle

`Abi1GoldenBytes`を実行し、次のv1.5.2以前の固定SHA-256を維持した。

```text
Sectioned Artifact
Frozen Leaf Package A
Frozen Leaf Package B
SGE4CMP 1.0 Frozen Composition
```

結果:

```text
ABI 1.x golden bytes passed.
```

### ABI 2.0 Portable self-test

D3D12 API objectを作らず、D3D12 Schema 17として完全に検証可能な外部Buffer二本のPortable Leaf Packageを生成した。同じLeaf二個から、Composition input -> internal -> outputの三Flowを構築した。

確認項目:

- Schema 17 Leaf Package build／read／D3D12 schema decode
- SGE4UNI 2.0を二回直接生成してbyte一致
- Portable Golden file bytes = 9304
- Portable Golden SHA-256 = `753c82dfc62c65cf56d09bccf81b9b081b2e9c9a04c81ddce2d0b79f36b77223`
- major 2／minor 0
- exact 8 direct Section
- 埋め込みLeaf bytesの完全一致
- ABI 2.0 Round-trip
- Core／Semantic／File digest一致
- SGE4UNI 1.1 Corpus生成
- Production ReaderによるABI 1.1拒否
- 明示的Migration ToolによるABI 1.1 -> 2.0変換
- 直接生成v2とMigration後v2のbyte完全一致
- Contract／Plan／Seal／Schedule／Recovery Set identity保存
- corruption corpus

Debug相当`-O0`とRelease相当`-O2 -DNDEBUG`の両方で実行した。

結果:

```text
ABI 2.0 portable self-test passed.
```

### ABI 2.0 corruption corpus

Reader 16ケースとWriter 1ケース、合計17のNegative Gateを実行した。

- 必須Section欠落
- 未知Required Section
- Manifest schema不一致
- 非dense Leaf ID
- 非Canonical Leaf offset
- 非zero Leaf padding
- Leaf Package破損
- Contract破損
- Verified Decision破損
- Verification Certificate破損
- Composition Core digest不一致
- Frozen Composition identity不一致
- Authority Ledger不一致
- Dynamic identity不一致
- descriptor offset範囲外
- descriptor kind重複
- Writerへの重複Section入力

### Migration Acceptance

```text
New SGE4統合移行受入試験に合格
再現した不変条件数: 40
```

## Windowsで必要な最終確認

Frozen Composition file bytes、Reader経路、Runtime load入力、資格試験の実Leaf経路を変更したため、次はWindows／MSVC／D3D12でのみ確定する。

```bat
run_new_sge4_full_gate.bat
```

- MSVC v145 Debug／Release build
- ABI 2.0 Sourceの全Static Library Link
- HLSL compile
- 実Schema 17 Frozen Leaf生成
- flat SGE4UNI 2.0生成
- SGE4INV 1.1生成
- ABI 1.x Oracle
- ABI 2.0 direct／migration／corruption
- Debug A／Debug B／Release SGE4UNI 2.0 byte一致
- WARP materialization／submission／readback
- Controlled whole-Composition Recovery
- stale epoch rejection
- Actual Device removal／removed-adapter exclusion
- Gate終了後のMSBuild node終了

このLinux環境では、上記Windows固有結果を通過済みとは記録しない。
