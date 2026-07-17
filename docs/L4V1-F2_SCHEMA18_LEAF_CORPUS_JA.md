# SGE4 Level 4 v1 F2 — Schema 18 Leaf Corpus

## 1. 目的

L4V1-F2は、F1で確立したD3D12 Schema 18 Composition Interfaceを、既存の54個の受理済みSemantic scenarioへ適用し、再現可能なSchema-18 Leaf Package corpusとして凍結する段階である。

F2が証明する範囲は次のとおりである。

- 既存Schema-17 corpusと同じ54個のsource scenarioから出発する。
- すべてのcaseをSchema 18 / Runtime 18 Leaf Packageとして生成できる。
- Composition endpointのaccessはauthorが指定せず、F1 CompilerがSemanticGraphから導出する。
- 同一プロセス内だけでなく、別DebugプロセスおよびDebug/Release間で54 Packageすべてがbyte-identicalになる。
- Schema 17の既存54-Package freezeとメインSolutionを変更しない。

F2はComposition Contract、Resource Flow Plan、Link Verifier、Composition Runtimeを実装しない。それらはF3以降の責務である。

## 2. Corpusの継承

F2のsource corpusは、`44_CanonicalFreezeTests`が保持する54 scenarioの構成を独立に再構築する。

構成は次のとおりである。

- Level 1 Slice corpus: 18 cases
- Stage G headless: 1 case
- Stage H general graph: 3 cases
- Stage J generated graph: 29 cases
- Stage K runtime scenarios: 3 cases
- 合計: 54 cases

旧source corpus identityは変更しない。

```text
43322a7c33d2ae82bfa577c0fee79ad7b915bdd8d38655eac1631a4e81c89d9b
```

この一致により、F2が別の都合のよい教材集合へすり替わっていないことを確認する。

## 3. Composition Leafへの境界正規化

F1のSchema-18 endpointは、次の二種類だけを受理する。

- ReadOnly: External BufferをShaderBuffer/SRVとして読む。
- WriteOnly: External BufferをStorageBuffer/UAVとして書く。

既存54ケースのうち51ケースは、そのままSchema-18 Leafへ昇格できる。Stage Kの3ケースだけは、既存のExternal outputがCopyDestinationであったり、同じExternal ResourceをWrite後にCopySourceとして読んだりするため、F1の厳格なendpoint ABIへ直接適合しない。

F2ではその3ケースだけに明示的な`ComputeExportProxy`を追加する。

1. 旧External outputをPackage-owned Persistent GPU-written Bufferへ内部化する。
2. 元のCompute/Copy/Temporal処理を変更せず完了させる。
3. 最後に内部BufferをSRVで読み、新しいExternal BufferへUAVで書く1段のCompute Workを追加する。
4. 新しいExternal BufferだけをWriteOnly Composition endpointとして公開する。

対象は次の3ケースである。

- `StageK/CrossQueueTemporal`
- `StageK/DynamicExternalDedicated`
- `StageK/DynamicExternalDirectFallback`

この変換はComposition runtimeの補助処理ではなく、Leaf Package内部に凍結された明示的なSemantic Workである。

## 4. 固定Corpus identity

```text
Source semantic corpus digest
43322a7c33d2ae82bfa577c0fee79ad7b915bdd8d38655eac1631a4e81c89d9b

Schema-18 Leaf semantic corpus digest
6ecafb273230ccf3e0ea0afe020fea440e5e26f9d4c7d330e1ef7922c5268d34

Endpoint authoring digest
1455e539474d5433d4e827013279b3ca7e8f7a4561a8c56fb71b1d451fff057a
```

固定cardinalityは次のとおりである。

```text
Cases:                 54
Normalized cases:       3
Composition endpoints: 12
ReadOnly endpoints:      9
WriteOnly endpoints:     3
```

Stable endpoint author keyは、sorted corpus ordinal、access、Semantic Resource IDから決定する。

```text
leaf/<two-digit ordinal>/<input|output>/<resource-id>
```

例:

```text
leaf/03/input/10
leaf/51/output/3
```

Schema 18 Packageには、このauthor keyそのものではなく、F1で規定したdomain-separated SHA-256 StableEndpointKeyが格納される。

## 5. プロジェクト境界

### `27A_Schema18LeafCorpus`

Corpus recipe、3件の境界正規化、stable key authoring、semantic identityを所有する。Compiler、Package Schema、Runtime、旧Prototype-A linkerへの依存を持たない。

### `46B_Schema18LeafCorpusTests`

54 caseをSchema 17とSchema 18の両方でコンパイルし、次を検証する。

- source corpus identity
- normalized Leaf corpus identity
- endpoint authoring identity
- Schema 18 / Runtime 18 header
- Schema-17 decoderによる外側Schema-18 Packageの拒否
- Schema-18 decoderによるendpointと埋め込みSchema-17 Packageの再検証
- 未正規化51ケースで、埋め込みSchema-17 bytesが旧compile結果と完全一致
- 54 Packageのfresh DebugおよびDebug/Release byte identity

### Solution

F2は独立Solutionを使用する。

```text
SemanticGpuEngine4_Level4v1_F2.sln
```

既存の`SemanticGpuEngine4.sln`は凍結基準として変更しない。

## 6. 実行

SGE4のリポジトリ直下で実行する。

```powershell
.\prepare_sge4_level4v1_f2.bat
.\run_sge4_level4v1_f2.bat
```

`prepare`は適用先commit、依存境界、F1境界、F2 corpus定数、スクリプト契約、SOURCE_MANIFESTを検証する。

`run`はDebug/Release build、F1 regression、54 Leaf PackageのDebug別プロセス2回とRelease 1回の生成、manifest比較、全Packageの名前・byte数・SHA-256比較を行う。

生成物は次に置かれる。

```text
build/tests/level4v1-f2/
```

## 7. F2完了条件

最終bannerが次の形で表示されること。

```text
SGE4 L4V1-F2 SCHEMA 18 LEAF CORPUS PASSED
Source corpus: same 54 accepted Semantic scenarios
Schema 18 corpus: 54 deterministic Leaf Packages
Boundary normalization: 3 explicit Compute export proxies
Endpoints: 12 total, ReadOnly 9, WriteOnly 3
Determinism: fresh Debug and Debug/Release byte identity
```

この時点で証明されるのは「Leaf単体のComposition Interfaceを持つ54 Package corpusが固定された」ことであり、複数Leaf間のresource flowや実行順序が正しいことではない。後者はF3以降で証明する。
