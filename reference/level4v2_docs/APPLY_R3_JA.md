# Level 4 v2 R3 適用手順

基点コミット:

```text
0caa4776077e58df6473e9f5760555b437bc5305
```

SGE4ルートへ上書き展開して実行します。

```powershell
.\run_sge4_5_level4v2_r3_prepare.bat
.\run_sge4_5_level4v2_r3_canonical_composition.bat
```

1本目はR3の5 ProjectをSolutionへ登録し、`SOURCE_MANIFEST.sha256`を再生成します。Git操作は行いません。

2本目はSpiral 7、R0、R1、R2のRegression gate、R3静的境界、Debug/Release build、7 graph scenario、24 negative case、Evidence byte equalityと固定SHA-256、Source Manifestを確認します。
