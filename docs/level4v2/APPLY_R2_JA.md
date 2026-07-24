# Level 4 v2 R2 適用手順

基点コミット:

```text
eee68ce6e9be5537d3041964679e55f4c5d2c448
```

SGE4ルートへ上書き展開して実行します。

```powershell
.\run_sge4_5_level4v2_r2_prepare.bat
.\run_sge4_5_level4v2_r2_unified_authority.bat
```

1本目はR2 ProjectをSolutionへ登録し、`SOURCE_MANIFEST.sha256`を再生成します。Git操作は行いません。

2本目はSpiral 7、R0、R1の回帰境界、R2の依存・construction access・単一事実所有、Debug/Release build、18件のmutation/replay拒否、deterministic Evidence、Source Manifestを確認します。
