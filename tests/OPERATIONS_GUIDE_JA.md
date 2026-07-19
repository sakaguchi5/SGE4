# SGE4 階層化テスト運用説明書

## 1. 目的

SGE4には、Semantic、Planning、Package、Runtime、D3D12、Recovery、Frontend、決定性を守る多数のテストがあります。

これらはLevel 1〜3で実証した性質を保持するために必要であり、削除するべきではありません。一方、編集のたびに完全Qualificationを実行すると、開発の待ち時間が増えます。

本テスト構成では、次を両立します。

```text
日常開発
  必要なテストだけを短い周期で実行する

Stage完成判定
  従来と同じ完全なQualificationを実行する
```

原則は次の一文です。

> テスト資産は減らさず、実行頻度と実行範囲を分ける。

---

## 2. 四段階の基本運用

### Tier 1: Dev

コマンド:

```bat
tests\run_dev.bat
```

用途:

- 数行から小さな機能単位の変更後
- 通常の編集ループ
- PlanningやPackage境界を壊していないか早く確認したいとき

実行内容:

- ProjectReferenceと禁止依存の静的検査
- Semantic contract
- Package contract
- Compiler pipeline
- Authority 17件

含まないもの:

- WARP実行
- Readback
- Device removal / recovery
- 54 Package canonical corpus
- Debug / Release横断比較
- fresh-process manifest比較

Authority 17件は実行時間に対して価値が高く、SGE4の最重要境界なのでDevに残しています。SOURCE_MANIFESTは編集中のファイル変更を意図的に許すためDevでは検査せず、Architecture、Gate、Regression、Freezeで検査します。

推奨頻度:

```text
小さな変更のまとまりごと
```

---

### Tier 2: Gate

コマンド:

```bat
tests\run_gate.bat
```

用途:

- コミット前
- 一日の作業を区切る前
- 小Stageの完了候補

実行内容:

- Architecture検査
- Debug Solution全体ビルド
- Semantic contract
- Package contract
- Compiler pipeline
- Planning
- Authority
- D3D12 operation conformance
- Adversarial boundary
- Frontend equivalence
- Slice scenario
- Metamorphic tests

含まないもの:

- WARP readback
- Device removal / recovery
- 54 Package canonical freeze
- Release build
- fresh-process比較

Gateは「GPU実行を待たずに、主要な設計面を広く検査する」位置づけです。

推奨頻度:

```text
コミット前
```

---

### Tier 3: Regression

コマンド:

```bat
tests\run_regression.bat
```

用途:

- Stage内の節目
- Compiler、Package、Runtimeをまたぐ変更後
- WARPでの実行結果も確認したいとき

実行内容:

- Architecture検査
- Debug全Solutionビルド
- 既存の `run_tests.bat Debug` 全内容
- 54 Package canonical corpus
- Planning manifest検査
- WARP semantic readback
- Slice execution
- D3D12 readbackとrecovery
- verified alternative Plan runtime

含まないもの:

- Release全Qualification
- Debug / Release byte-identical比較
- fresh-process二重生成比較

推奨頻度:

```text
小Stageの完成時
大きな責務変更の後
```

---

### Tier 4: Freeze

コマンド:

```bat
tests\run_freeze.bat
```

用途:

- Stage完成の正式判定
- 配布ZIPを作る前
- 基準manifestやdigestを固定する前

実行内容:

- `run_sge4_5_stage0d.bat` の完全Qualification
- Debug全テスト
- Release全テスト
- Authority gate
- fresh Debug processでのCanonical manifest二重生成
- Debug / Release Canonical manifest一致
- fresh processでのPlanning manifest二重生成
- Debug / Release Planning manifest一致
- WARP、Readback、External、Temporal、Recovery

Freezeは重いことが正常です。日常開発では使用せず、完成判定に限定します。

推奨頻度:

```text
Stage完成時のみ
```

---

## 3. 変更領域別Suite

四段階のほかに、変更した責務だけを確認するSuiteがあります。

### Architecture

```bat
tests\run_architecture.bat
```

検査対象:

- PowerShell構文、UTF-8 BOM、CRLFのスクリプト契約
- Visual Studio ProjectReferenceの存在
- 依存循環
- 禁止された依存方向
- CandidatePlannerがPackage-freeであること
- `LowerVerifiedPlan`の利用者制限
- Compatibility Oracleの隔離
- SOURCE_MANIFESTの欠落、余分、digest変更

実行タイミング:

- `.vcxproj`変更
- include変更
- プロジェクト追加
- Compiler責務の移動
- スクリプトや資料の追加後

---

### Semantic

```bat
tests\run_semantic.bat
```

検査対象:

- Semantic Model
- Semantic Builder
- Semantic Analysis
- ResourceUse、Work、Program Interfaceの基本契約

実行タイミング:

- `02_SemanticModel`
- `03_SemanticBuilder`
- `04_SemanticAnalysis`
- Semanticを生成するScenario

---

### Planning

```bat
tests\run_planning.bat
```

検査対象:

- Candidate生成
- Cost計算
- Policy / Profile選択
- SGE3 planningとの非退行
- VerifiedExecutionPlan
- Authority 17件

実行タイミング:

- `05A_CompilationInput`
- `06_ExecutionPlanModel`
- `07_ExecutionPlanVerifier`
- `08_CandidatePlanner`
- `12_SGE4_5Compiler`の選択処理

---

### Authority

```bat
tests\run_authority.bat
```

検査対象:

- raw PlanのLowering禁止
- seal identity
- Binding、Descriptor、Signal、AllocationのPlan権威
- Profile再選択と拒否条件

実行タイミング:

- Verifier seal
- Plan identity
- Lowering入口
- Profile情報

Planning全体より狭く、Authorityだけを素早く再確認したい場合に使います。

---

### Package

```bat
tests\run_package.bat
```

検査対象:

- Frozen Package container
- D3D12 Schema 17
- Serialization / deserialization
- Compiler pipeline
- Backend operation対応表

実行タイミング:

- `09_FrozenPackageCore`
- `10_D3D12PackageSchema`
- `11_D3D12PackageLowering`
- Package validation

このSuiteは54 corpusを含みません。全corpus確認はCorpusまたはRegressionを使います。

---

### Runtime

```bat
tests\run_runtime.bat
```

検査対象:

- Package生成
- WARP semantic execution
- Slice execution
- Readback
- fresh-process rematerialization
- Device recovery
- verified alternative Plans

実行タイミング:

- `13_PackageRuntime`
- `14_D3D12Backend`
- External / Temporal / Recovery
- Operation実行処理

このSuiteはGPU/WARPを使うため、Devより重いSuiteです。

---

### Frontend

```bat
tests\run_frontend.bat
```

検査対象:

- Classical / SDF / direct PGAの等価性
- Slice scenarioのSemanticとPackage構造

実行タイミング:

- `20_ExperimentDomain`
- `21_ClassicalFrontend`
- `22_SdfFrontend`
- `23_PgaFrontend`
- `24_SliceScenarios`

---

### Corpus

```bat
tests\run_corpus.bat
```

検査対象:

- Metamorphic transformations
- Generated graph corpus
- 54 canonical Packages
- Semantic corpus digest
- SGE3 canonical Package非退行

実行タイミング:

- 一般Graph処理
- canonical ordering
- ID割当
- serialization
- digest計算
- Package bytesに影響する変更

---

### Adversarial

```bat
tests\run_adversarial.bat
```

検査対象:

- 破損Package
- 不正な境界条件
- overflow、範囲外、参照不整合
- Runtime boundary rejection

実行タイミング:

- Reader / Writer
- Schema validator
- Runtime入力検証
- External boundary

---

## 4. 変更内容から選ぶ早見表

| 主な変更箇所 | 最初に実行 | コミット前 | Stage節目 |
|---|---|---|---|
| `.vcxproj`、依存方向 | Architecture | Gate | Regression |
| Semantic Model / Analysis | Semantic | Gate | Regression |
| ExecutionPlan / Verifier | Planning | Gate | Regression |
| Seal / Authority | Authority | Gate | Regression |
| Candidate / Policy / Profile | Planning | Gate | Regression |
| Package Core / Schema | Package | Gate + Corpus | Regression |
| Package Lowering | Package + Authority | Gate + Corpus | Regression |
| Runtime / Backend | Runtime | Gate | Regression |
| External / Temporal / Recovery | Runtime | Gate | Regression |
| Classical / SDF / PGA | Frontend | Gate | Regression |
| Canonical order / digest | Corpus | Gate | Regression |
| Stage完成 | DevまたはGate | Regression | Freeze |

変更が複数領域にまたがる場合は、該当Suiteを複数実行するかGateを使います。

---

## 5. 推奨する日常手順

### 小さな編集

```text
1. 対象領域のSuiteを実行
2. 続けて開発
3. まとまりができたらDev
```

例:

```bat
tests\run_planning.bat
tests\run_dev.bat
```

### コミット前

```bat
tests\run_gate.bat
```

### Stageの小区切り

```bat
tests\run_regression.bat
```

### Stage完成

```bat
tests\run_freeze.bat
```

Freezeに成功した結果だけを正式な完成判定として扱います。

---

## 6. DebugとRelease

通常はDebugを使います。

```bat
tests\run_package.bat
```

これは次と同じです。

```bat
tests\run_package.bat Debug
```

局所的にReleaseを確認する場合:

```bat
tests\run_package.bat Release
```

ただしReleaseを一部だけ通しても正式Freezeにはなりません。正式なDebug / Release一致は `run_freeze.bat` で確認します。

---

## 7. `-NoBuild` の利用

中央Runnerを直接呼べば、既にビルド済みの実行ファイルだけでテストできます。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\Invoke-SGE4_5Tests.ps1 `
  -Suite Planning `
  -Configuration Debug `
  -NoBuild
```

使用してよい場面:

- ソースを変更していない
- 同じbuild出力でテストだけ再実行したい
- 失敗を再現したい

使用してはいけない場面:

- ソース変更後
- 構成を切り替えた後
- Stage完成判定
- Freeze

古いexeを実行する危険があるため、通常の`.bat`はBuildを省略しません。

---

## 8. SOURCE_MANIFESTの運用

`SOURCE_MANIFEST.sha256`は、配布物のファイル集合と内容を固定します。

Architecture Suiteは次を拒否します。

- PowerShell構文エラーとスクリプト文字コード／改行契約違反
- Manifestにあるファイルの欠落
- Manifestにない余分なファイル
- ファイル内容のdigest変更

Visual Studioが生成する個人・端末固有ファイルは配布物ではないため、SOURCE_MANIFESTの対象外です。

```text
*.user
*.suo
*.VC.db
*.VC.opendb
.vs/
```

`.gitignore`とSOURCE_MANIFESTは別の仕組みなので、両方で同じ生成物を除外します。

ソース、スクリプト、説明書を意図的に変更した場合のみ、次を実行します。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\tools\Update-SourceManifest.ps1
```

その直後に必ず次を実行します。

```bat
tests\run_architecture.bat
```

注意:

> テスト失敗を消すためにManifestを更新してはいけません。変更内容を確認し、その変更を配布物として正式に採用すると決めた場合だけ更新します。

---

## 9. ログの扱い

全Suiteは次にログを保存します。

```text
docs/test-logs/YYYYMMDD-HHMMSS-Suite-Configuration.log
```

ログは一時的な`build`成果物ではなく、Qualificationの実行証拠としてGitへコミットします。
`docs/test-logs/*.log`は実行ごとに生成されるためSOURCE_MANIFESTの固定対象からは除外しますが、`.gitignore`では除外しません。

失敗報告には次を残します。

```text
実行したSuite
Debug / Release
最後のPASS行
最初の失敗行
ログファイル
変更したプロジェクト
```

Stage Freezeのmanifestは従来どおり次に生成されます。

```text
build/sge4-stage0d/
```

---

## 10. 失敗時の切り分け

### Architectureで失敗

最初に修正します。ほかのテストを続行しません。

主な原因:

- ProjectReference漏れ
- 循環依存
- CandidatePlannerへのPackage依存漏出
- SGE4_5Compiler以外からのLowering
- SOURCE_MANIFEST未更新

### Devで失敗

失敗した最小領域Suiteを単独実行します。

例:

```bat
tests\run_authority.bat
tests\run_package.bat
```

### Gateだけ失敗

局所単体は通るが、責務間の統合で壊れています。最後に通ったSuiteと最初に失敗したSuiteの境界を確認します。

### Regressionだけ失敗

主に次を疑います。

- WARP実行
- Readback差
- Recovery
- Generated graph
- Canonical corpus

### Freezeだけ失敗

主に次を疑います。

- Debug / Release非決定性
- fresh-process非決定性
- manifest identity
- 時刻、address、unordered iterationなどの混入

Freeze失敗時に期待digestを安易に更新しません。まず意味変更か、非決定性か、identity定義ミスかを切り分けます。

---

## 11. 新しいテストを追加する規則

新しいテストには、最低限次を決めます。

```text
Invariant
  何を守るテストか

Owner
  どの責務・プロジェクトが所有するか

Suite
  どの用途別Suiteへ入るか

Tier
  Dev / Gate / Regression / Freezeのどこで必要か

Failure meaning
  失敗が何を意味するか
```

同じInvariantを検査するテストを無条件に増やしません。

追加する前に確認します。

```text
既存テストで検査できないか
既存テストへ一ケース追加できないか
独立exeにする必要があるか
毎回実行する必要があるか
Freeze限定でよいか
```

テスト数ではなく、Invariantの網羅性を管理します。

---

## 12. 中央Runnerの直接利用

すべての`.bat`は次を呼びます。

```text
tests/Invoke-SGE4_5Tests.ps1
```

直接利用例:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\Invoke-SGE4_5Tests.ps1 `
  -Suite Runtime `
  -Configuration Debug
```

利用可能なSuite:

```text
Dev
Gate
Regression
Freeze
Architecture
Semantic
Planning
Authority
Package
Runtime
Frontend
Corpus
Adversarial
```

通常は`.bat`を使えば十分です。

---

## 13. 基本方針の要約

```text
編集中
  対象領域Suite

小さなまとまり
  Dev

コミット前
  Gate

Stageの節目
  Regression

Stage完成
  Freeze
```

完全テストを削除せず、必要なタイミングへ移したことがこの運用の核心です。


## Stage 03 Research Contract Freeze

Foundation Bootstrap合格後、次を実行する。

```powershell
.\run_sge4_5_stage03_contract.bat
```

このGateはC++機能を実行せず、F0 code baseline、Research Contract digest、S00-S15、S1-I01からS1-I18、Project境界を検査する。
