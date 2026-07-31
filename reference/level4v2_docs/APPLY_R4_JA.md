# Level 4 v2 R4 適用手順

基点コミット:

```text
09b56250c88bd52b7e02a2510cd6cf2b7e814bde
```

SGE4ルートへ上書き展開し、次を実行します。

```powershell
.\run_sge4_5_level4v2_r4_dynamic_invocation_history.bat
```

本体runnerがR4 Project登録、SOURCE_MANIFEST再生成、R0～R3 regression、Debug/Release build、8シナリオ、27拒否、Evidence一致を順番に実行します。Git操作は行いません。
