# Level 4 v2 R6 適用手順

SGE4ルートへZIPを上書き展開し、次を実行します。

```powershell
.\run_sge4_5_level4v2_r6_migration_corpus.bat
```

runner自身が次を行います。

1. R6 Project登録とSolution構成の正規化
2. `SOURCE_MANIFEST.sha256`再生成
3. Spiral 7 Frozen ReferenceとR0-R5 Regression
4. R6 manifest・40 Invariant ledger・Evidence lane境界の静的検証
5. Debug／Release build
6. 26 Migration caseと20 rejection caseの実行
7. Debug／Release Evidenceのbyte一致と固定SHA-256確認
8. Source Manifest確認

R6はSpiral 7 CU6性能測定を再実行しません。外部Evidenceは追跡済みSHA-256によって参照されます。
