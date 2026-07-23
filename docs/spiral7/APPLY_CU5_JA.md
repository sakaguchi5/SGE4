# Spiral 7 CU5 最終版 適用手順

基点コミット:

```text
67cb40b5204e1e06ecac576206ba969ec2db02b6
```

ZIPをSGE4ルートへ上書き展開します。準備処理は、旧中間文書 `docs/spiral7/CU5_EXECUTION_OPTIMIZATION_V2.md` を削除し、Project登録を確認してSOURCE_MANIFESTを再生成します。

```powershell
.\run_sge4_5_spiral7_cu5_prepare.bat
```

通常は次の正式回帰gateを実行します。

```powershell
.\run_sge4_5_spiral7_cu5_architecture_qualification.bat
```

通常gateはDebugの4 Invocation WARP smokeと、Releaseの完全128 Invocation Qualification、Controlled Recovery、RemoveDevice、Fresh rematerializationを実行し、Release証拠が受理済み完全監査SHA-256と一致することを要求します。

Architecture、証拠serializer、コンパイラ／ツールチェーン、Recovery、Device epoch、D3D12実行経路を変更した場合、またはOwnerが再監査を要求した場合は次を実行します。

```powershell
.\run_sge4_5_spiral7_cu5_exhaustive_audit.bat
```

完全監査は従来どおりDebug／Releaseの全経路を実行するため長時間かかります。日常回帰には使用しません。

通常gate成功時:

```text
SGE4-5 SPIRAL 7 CU5 ROUTINE REGRESSION PASSED
SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE EVIDENCE PRESERVED
```

完全監査成功時:

```text
SGE4-5 SPIRAL 7 CU5 EXHAUSTIVE DETERMINISM AUDIT PASSED
SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE EVIDENCE RECONFIRMED
```
