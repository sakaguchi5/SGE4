# Spiral 7 CU2 適用手順

このZIPは、Spiral 7 CU1が成功したSGE4-5フォルダへ上書きする差分です。

削除ファイルはありません。Git操作も行いません。

## 1. ZIPを上書き展開

`172_...`、`173_...`、`174_...`、`182_...`の各新規Projectと、`docs`、`tests`、二つのrunnerをSGE4-5ルートへ配置してください。

## 2. Solution登録とManifest再生成

PowerShellから実行します。

```powershell
.\run_sge4_5_spiral7_cu2_prepare.bat
```

この処理は四つのProjectを`SemanticGpuEngine4-5.sln`へ重複なしで登録し、`SOURCE_MANIFEST.sha256`を再生成します。

## 3. CU2実行

```powershell
.\run_sge4_5_spiral7_cu2_compact_delta_history.bat
```

成功時には次が表示されます。

```text
SGE4-5 SPIRAL 7 COMPLETION UNIT 2 PASSED
```

Debug／Releaseの双方をビルドし、14個のWARP transition caseとbyte-identical evidenceを確認します。
