# SGE4 L4V1-F1 — Schema 18 Composition Interface

## 基準

この差分は、次のコミットだけを基準にしています。

```text
e3730feb90be8690e1cc4e9d937c7c3b11899f89
```

既存のSchema 17、Runtime 17、54個のLeaf Package、Prototype-Aの
`16_CompositionContract`〜`19_PackageLinker`および`29_CompositionRuntime`は変更しません。

## F1の証明対象

F1は、Leaf Package自身がCompositionへ公開するBuffer endpointについて、次をFrozen ABIへ固定します。

- stable endpoint key
- External slot identity
- Package Resource identity
- Buffer kind、format、minimum byte size
- `ReadOnly`または`WriteOnly`
- required incoming state
- guaranteed outgoing state
- completion-token synchronization
- per-frame semantics

Composition作者はaccess方向を指定できません。`12A_CompositionLeafCompiler`が、検証済みSemanticGraphの
`ResourceUse::effect`と`ViewRole`からaccessを導出します。

## Schema 17とSchema 18の関係

通常の`12_SGE4Compiler::CompileCanonical`は、従来どおりSchema 17を生成します。

Schema 18を生成するには、明示的に次を呼びます。

```cpp
auto result = sge4::compiler::composition::CompileCompositionLeafCanonical(
    graph,
    targetProfile,
    {
        {inputResource,  "frame/input"},
        {outputResource, "frame/output"}
    });
```

stable keyは1〜127文字で、使用可能文字は次です。

```text
A-Z a-z 0-9 . _ - /
```

文字列はdomain-separated SHA-256へ変換され、Frozen Packageには256-bit keyとして保存されます。
宣言順序はPackage bytesへ影響しません。Endpoint順序はSchema-17 External slotのcanonical順序です。

## F1で許可するaccess

### ReadOnly

```text
Effect::Read
ViewRole::ShaderBuffer
ProgramParameterKind::ReadOnlyBuffer
Frozen ViewClass::ShaderResource
```

ReadOnly endpointはincoming stateとoutgoing stateが同一でなければなりません。

### WriteOnly

```text
Effect::Write
ViewRole::StorageBuffer
ProgramParameterKind::UnorderedBuffer
Frozen ViewClass::UnorderedAccess
```

### 拒否

F1では次を拒否します。

- `Effect::ReadWrite`
- 同一External ResourceでRead useとWrite useが混在
- Endpointとして未宣言のExternal Buffer
- 一つのExternal Bufferに複数宣言
- stable keyの重複
- stable key digestの重複
- SRVをWriteOnlyと宣言したEndpoint表
- UAVをReadOnlyと宣言したEndpoint表
- External slot、Resource、state、sizeを書き換えたEndpoint表

## Decoderの独立検査

`d3d12_v18::CompositionLeafView::Decode`は、Endpoint表だけを信用しません。

1. Schema 18 containerとRuntime 18要求を確認する。
2. Composition Endpoint Tableをdecodeする。
3. Endpoint Tableを除いた全SectionからSchema-17 Leafを再構築する。
4. 既存のSchema-17 `D3D12PackageView::Decode`で全Leaf契約を再検証する。
5. Endpoint表とExternal slot、Resource、View、state、sizeを完全照合する。

従来のSchema-17 DecoderはSchema 18を拒否します。F1ではRuntime 18実行経路を追加していません。
これは意図的です。F1の責務はLeaf Composition ABIの確立までです。

## Section形式

Composition Endpoint TableのSection kindは次です。

```text
0x00001012
```

record strideは96 bytesです。Sectionは`Required | ExecutionAffecting`です。
Core Package Formatへ正式な既知Sectionとして登録され、Schema-18 Decoderがrecord意味を検証します。

## Solution統合方針

既存の`SemanticGpuEngine4.sln`はPrototype-AのFreeze境界として変更しません。
F1では次の独立Solutionを追加します。

```text
SemanticGpuEngine4_Level4v1_F1.sln
```

Qualification runnerはSolution全体ではなく`46A_Schema18CompositionInterfaceTests.vcxproj`を直接buildし、
ProjectReferenceを通じて必要な既存projectだけをbuildします。

`verify_dependencies.ps1`に加え、`tests/Verify-Level4v1F1Boundaries.ps1`が10A／12Aの直接依存を固定します。

## 証明境界

WriteOnly／ReadOnlyは、SGE4の既存source-authorityモデルに従い、検証済みSemanticGraphの
`ResourceUse::effect`、`ViewRole`、`ProgramParameterKind`とFrozen LeafのView classを照合して確定します。
任意のHLSL命令列を逆解析してread/write side effectを推論する段階ではありません。
これは既存CompilerがProgramInterfaceをShader契約の権威として扱う境界を維持したものです。

Schema-17ヘッダーへSchema-18 Endpoint Sectionを混入した再署名PackageはCore PackageReaderが拒否します。
Schema-18 Packageへ未知のoptional Sectionを追加した場合もSchema-18 Decoderが拒否します。

## 適用手順

ZIPの中身をコミット`e3730f...`のリポジトリ直下へ展開してください。

最初に一度だけ実行します。

```powershell
.\prepare_sge4_level4v1_f1.bat
```

この処理は、Git HEADまたは基準ファイルのSHA-256を確認し、`SOURCE_MANIFEST.sha256`を現在の完全なtreeから再生成します。

その後、F1 qualificationを実行します。

```powershell
.\run_sge4_level4v1_f1.bat
```

実行内容は次です。

- dependency boundary check
- script contract check
- source inventory check
- Debug build
- Release build
- fresh Debug processを2回実行
- Release processを1回実行
- Schema-18 artifactのDebug／Release byte identity
- positive／negative authority tests

## F1完了banner

```text
SGE4 L4V1-F1 SCHEMA 18 COMPOSITION INTERFACE PASSED
```

このbannerはLevel 4 v1全体の完成を意味しません。次のF2でSchema-18 Leaf corpusを構築するためのABI gateです。
