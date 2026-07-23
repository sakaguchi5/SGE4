# Spiral 7 CU4 適用手順

基点コミットは `21e7e91e1f85bb5620fca50e9def76b7c9a76291` です。CU3が成功した作業フォルダのルートへ、このZIPを上書き展開します。

```powershell
.\run_sge4_5_spiral7_cu4_prepare.bat
.\run_sge4_5_spiral7_cu4_candidate_family.bat
```

成功時には `SGE4-5 SPIRAL 7 COMPLETION UNIT 4 PASSED` と証拠SHA-256が表示されます。

CU4は勝者を選びません。A/B/Cの意味結果が各Invocationでbyte一致すること、Candidate CのBlock masksが正確な`W_t/R_t`を表すこと、RuntimeがCandidateを選ばないことを検証します。
