# Spiral 7 CU5 適用手順

基点コミット:

```text
8f3b17e25d2a500beb2b658e6bc0a1d3f646ec26
```

CU5 ZIPをSGE4ルートへ上書き展開します。削除ファイルはありません。

最初にProject登録とSOURCE_MANIFEST更新を実行します。

```powershell
.\run_sge4_5_spiral7_cu5_prepare.bat
```

続いてCU5 gateを実行します。

```powershell
.\run_sge4_5_spiral7_cu5_architecture_qualification.bat
```

gateは次を実行します。

1. CU1〜CU5静的境界検証
2. Debug／Releaseビルド
3. 128 Invocation／384 Candidate WARP Architecture Qualification
4. Controlled Recoveryと旧epoch handle拒否
5. `A_t/M_t`明示rebindと全Active exact-generation再構築
6. 実際の`ID3D12Device5::RemoveDevice` quarantine
7. Fresh-process rematerialization
8. Debug／Release証拠bytes一致

成功時:

```text
SGE4-5 SPIRAL 7 COMPLETION UNIT 5 PASSED
SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE
```

## 実行最適化V2

V2は証明内容を減らさず、A/B/CをInvocation単位で一括送信します。128 InvocationのQualificationでは、Candidate実行回数384回は維持したまま、Fence待ちを384回から128回へ削減します。固定容量Bufferも全Timelineで再利用します。実行中は16 Invocationごとに進捗が表示されます。
