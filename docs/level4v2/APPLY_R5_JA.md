# R5 適用手順

SGE4ルートへ上書き展開し、次の1本を実行する。

```powershell
.\run_sge4_5_level4v2_r5_runtime_recovery.bat
```

runnerはR5 Project登録、SOURCE_MANIFEST再生成、R0-R4 regression、Canonical Debug/Release、Windows WARP Debug/Release、Evidence一致を順番に実行する。Git操作は行わない。
