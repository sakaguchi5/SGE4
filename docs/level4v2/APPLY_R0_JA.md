# Level 4 v2 R0 適用手順

基点コミット:

```text
a44b2ee3e5e151eab2f25f94e92d838b7931a4d9
```

SGE4ルートへ上書き展開して実行します。

```powershell
.\run_sge4_5_level4v2_r0_prepare.bat
.\run_sge4_5_level4v2_r0_input_freeze.bat
```

1本目はGit操作を行わず、`SOURCE_MANIFEST.sha256`を再生成します。

2本目は、Spiral 7閉鎖済み参照gate、R0 source freezeのSHA-256、carried invariant 40件のID・所有者・移行先、R1 vocabulary entry contract、v2実装Projectがまだ追加されていないこと、Runtime policy authorizationが`None`のままであること、Source Manifestを確認します。

成功時:

```text
SGE4 LEVEL 4 V2 R0 INPUT FREEZE PASSED
R0 COMPLETE
NEXT: R1 CANONICAL VOCABULARY
```
