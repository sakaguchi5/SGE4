# Spiral 7 CU1 適用手順

基点コミットは次です。

```text
d0bb0d406fca8beabed2331daff870ea414dd87d
```

1. ZIPの内容をSGE4-5リポジトリ直下へ上書き展開します。
2. Git作業ツリーに意図しない未追跡ファイルがないことを確認します。
3. PowerShellから次を実行します。

```powershell
.\run_sge4_5_spiral7_cu1_prepare.bat
```

このrunnerは現在の作業ツリーを基に`SOURCE_MANIFEST.sha256`を再生成してからCU1を検証します。

成功時には次が表示されます。

```text
SGE4-5 SPIRAL 7 COMPLETION UNIT 1 PASSED
```

CU1は文書と検証だけです。C++、Solution、ABI、Runtime、Backendは変更しません。
