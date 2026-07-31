# Level 4 v2 R1 適用手順

基点コミット:

```text
b8b5b4f675e4186a3ae202d718e091b63c53e264
```

SGE4ルートへ上書き展開して実行します。

```powershell
.\run_sge4_5_level4v2_r1_prepare.bat
.\run_sge4_5_level4v2_r1_canonical_vocabulary.bat
```

1本目はR1の2 Projectをsolutionへ登録し、`SOURCE_MANIFEST.sha256`を再生成します。Git操作は行いません。

2本目はSpiral 7参照gate、R0 regression、R1静的境界、Debug/Release build、両構成の自己テストEvidence byte一致、Source Manifestを確認します。

成功時:

```text
SGE4 LEVEL 4 V2 R1 CANONICAL VOCABULARY PASSED
R1 COMPLETE
NEXT: R2 UNIFIED AUTHORITY CHAIN
```
