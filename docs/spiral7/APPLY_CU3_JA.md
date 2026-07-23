# Spiral 7 CU3 適用手順

CU2成功済みのSGE4-5フォルダへZIPを上書き展開します。

PowerShellから次を実行します。

```powershell
.\run_sge4_5_spiral7_cu3_prepare.bat
.\run_sge4_5_spiral7_cu3_independent_authority.bat
```

成功時は次が表示されます。

```text
SGE4-5 SPIRAL 7 COMPLETION UNIT 3 PASSED
```

`prepare`はSolution登録とSOURCE_MANIFEST再生成だけを行います。Git操作は行いません。
