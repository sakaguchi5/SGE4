# Semantic GPU Engine 4-5

この独立ファイル群は `SGE4v1.zip` を直接基準にしています。最初に `run_sge4_5_foundation.bat` を実行してください。詳細は `docs/foundation/FOUNDATION_BOOTSTRAP.md` を参照してください。

# Semantic GPU Engine 4

SGE4は、完成済みSGE2を教材・互換性Oracleとして固定し、第三世代の責任境界で再構成した独立Visual Studioソリューションです。

```text
Solution : SemanticGpuEngine4-5.sln
namespace: sge4
Toolset  : Visual Studio 2026 / v145
Standard : /std:c++latest
Platform : x64 Debug / Release
Target   : D3D12 Package Schema 17 / Runtime 17
```

SGE2のVisual Studioプロジェクト、namespace、binary、repositoryへの製品依存はありません。SGE2で分かれていたLevel 2とLevel 3のコンパイラ入口は廃止し、次の一本の経路へ統合しています。

```text
SemanticGraph
  -> SemanticAnalysis
  -> SemanticObligation
  -> CandidatePlanner
  -> ExecutionPlanIR候補群
  -> 独立VerifyAndSeal
  -> VerifiedExecutionPlan
  -> D3D12PackageLowering
  -> FrozenExecutablePackage
  -> PackageRuntime
  -> D3D12Backend
```

CanonicalSafeも旧来の直通コンパイラではありません。候補空間を1件に制限したPolicyとして、同じPlanner、Verifier、sealed Loweringを通ります。

## 完全試験

PowerShellで適用する差分パッケージではありません。ZIPを展開したディレクトリが、そのまま完全な新規ソリューションです。

```powershell
cmd /c .\run_sge4_5_freeze.bat
```

この一つのコマンドで、次をDebugとReleaseの両方について検証します。

- 44プロジェクトの依存方向と循環禁止
- Semantic、Package、Compiler、Planning、Authority試験
- SGE2由来の固定54 PackageコーパスとのCanonical互換性
- fresh-process、Debug、Release manifestのbyte一致
- WARP実行、Buffer／Texture Readback
- Raster、Compute、Copy、複数Queue、handoff
- FrameLocal、Temporal、Aliasing、External Acquire／Release
- controlled recoveryと実Device removal／DRED／再物質化
- Classical、SDF、PGA frontendの観測等価性
- 検証済み代替Schedule／Queue Planの観測等価性

## Authority境界

公開Lowererは`VerifiedExecutionPlan`だけを受け取ります。生の`ExecutionPlanIR`をPackageへLoweringする通常APIはありません。

SGE2の直接Canonical経路は互換性試験専用のinternal entryとして残し、`28_SGE3CompatibilityOracle`だけが参照できます。`verify_dependencies.ps1`はProjectReferenceだけでなく、このinternal headerが他のソースへ漏れていないことも検査します。

## ABI方針

最初のSGE4 freezeは、SGE2が実機試験で証明した`SGE2PKG`、D3D12 Schema 17、Runtime 17のwire ABIを意図的に保持します。これはSGE2製品コードへの依存ではなく、同じPackage bytesが同じ意味を持つという互換性定理です。

将来SGE4固有ABIへ進む場合は、Magicだけを改名するのではなく、Container、Schema、Runtimeのversion付き変更として導入します。

## 個別コマンド

```powershell
cmd /c .\build.bat Debug
cmd /c .\build.bat Release
cmd /c .\run_tests.bat Debug
cmd /c .\run_authority_gate.bat
cmd /c .\run_demo.bat Debug
```


## Stage 0D Freeze修正

SGE4固有のmanifest identityと、SGE3から凍結継承するsemantic contract identityを分離しました。これにより、Solution／namespaceの世代名だけではsemantic corpus digestが変化しません。期待digest `43322a7c33d2ae82bfa577c0fee79ad7b915bdd8d38655eac1631a4e81c89d9b`は維持し、誤って発生した世代依存digestへ更新していません。

## 階層化テスト運用

日常開発で完全Qualificationを毎回実行しないため、`tests`フォルダに用途別Runnerを追加しています。

```bat
run_sge4_5_dev.bat
run_sge4_5_gate.bat
run_sge4_5_regression.bat
run_sge4_5_freeze.bat
```

変更領域別の個別Suiteと詳しい運用は、次を参照してください。

```text
tests/README_JA.md
tests/OPERATIONS_GUIDE_JA.md
```

## ファイル名の互換性

ZIP展開時の文字コード差を避けるため、リポジトリ内のファイル名とフォルダ名はASCIIに限定しています。日本語の本文はUTF-8のまま保持されます。Spiral 1完成仕様は `docs/spiral1/SGE4-5_Spiral1_Completion_Spec_v0.2.md` です。
