# CU6-2 適用・実行

基点コミット:

```text
903e00be0f7e39ffd1758004c117fbc0233b0164
```

ZIPをSGE4ルートへ上書き展開し、次を実行します。

```powershell
.\run_sge4_5_spiral7_cu6_prepare.bat
.\run_sge4_5_spiral7_cu6_measurement_decision_evidence.bat
```

通常実行は、160ケースのCanonicalSurfaceと100ケースのHighTransitionRefinementを順に測定し、paired authorityによる統合Decision Mapを生成します。

長時間無表示にはならず、各測定Blockについて`[MEASURED] pass=...`を表示します。

成功時:

```text
SGE4-5 SPIRAL 7 COMPLETION UNIT 6-2 PASSED
SGE4-5 SPIRAL 7 EXPERIMENT COMPLETE
SPIRAL 7 CLOSURE: OWNER REQUIRED
```
