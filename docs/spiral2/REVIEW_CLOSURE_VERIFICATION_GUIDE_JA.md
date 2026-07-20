# Spiral 2 レビュー指摘 1〜6 検証ガイド

状態: Owner execution required

この文書は、Spiral 2プロトタイプを直ちに「完成」と宣言する文書ではない。レビューで未解決だった六項目を、OwnerがWindows / Visual Studio / D3D12環境で再現・確認するための入口を固定する。

## 一括実行

リポジトリ直下のPowerShellまたはコマンドプロンプトから実行する。

```powershell
.\run_sge4_5_spiral2_review_closure.bat
```

実GPU計測まで含める場合:

```powershell
.\run_sge4_5_spiral2_review_closure.bat -IncludeRealGpu
```

通常実行はDebug/ReleaseのAuthority、Package、WARP、Recovery、Evidence V2 formatを検査する。`-IncludeRealGpu`は最後に候補別の実GPU Materialization / End-to-End / GPU timestampを採取する。

## 1. Verified operation列がそのままLeafへLoweringされること

正本:

- `85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.h`
- `85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.cpp`
- `86_Spiral2LeafCompiler/Spiral2LeafCompiler.cpp`

確認点:

- Verifierは`VerifiedHierarchyExecutionPlanV1`と`VerifiedHierarchyOperationV1`を生成する。
- Leaf Compilerは`plan.operations`を順番に走査し、各operationを一つのLeafへLoweringする。
- Leaf group全体を`CandidateKind`から別途組み直す`switch(group.kind)`は存在しない。
- VerifierはPlannerも`candidate::ProgramIdentityV1`も呼ばない。

実行証拠:

- `92_Spiral2CandidateGraphTests.exe`
- `93_Spiral2AuthorityMutationTests.exe`
- `94_Spiral2LeafPackageTests.exe`

## 2. Verified PlanとPackageのOperation / Program / Binding / dispatchが一致すること

`FrozenHierarchyLeafV1`は次を保持する。

- Verified Representation certificate
- Verified operation identity
- Verified Program template identity
- Package execution digest
- Package Program interface digest
- Package Binding Layout digest
- Package Shader bytecode digest
- Package binding identity
- verified dispatch dimensions

`SealFrozenHierarchyLeafBindingV1`は、許可済みの`12_SGE4_5Compiler` facadeを通じてPackageを再読込する。Target Schemaの復号はSchemaを正当に所有する`11_D3D12PackageLowering`内で行い、実際の`ExecuteCompute`、Compute command、Program、Binding Layout、Compute Shaderの証拠だけをLeaf Compilerへ返す。Leaf CompilerはこれらをVerified operation情報と一つの`packageBindingIdentity`へ束縛し、`10_D3D12PackageSchema`へ直接依存しない。

`94_Spiral2LeafPackageTests`は9 Leafについて、operation、Program template、dispatch、Program interface、Binding Layout、Shader digest、binding identityの改変を合計63件拒否する。

## 3. 階層dispatchモデルが設計と実装で同じこと

採用モデル:

```text
HierarchyExecutionModelV1::SingleDispatchCanonicalOrderSerial
```

意味:

- 階層Leaf一つにつき`ExecuteCompute`は一回。
- thread-group countは`1,1,1`。
- Shaderのthread 0が`canonicalBoneOrder`を順番に処理する。
- `hierarchyDepthCount`はFrozen意味情報だが、実dispatch数として偽装しない。

候補別のLeaf dispatch数:

```text
A MatrixHierarchy     3
B DirectPgaHierarchy  2
C HybridHierarchy     3
```

これは「depthごとの複数dispatch」方式ではない。将来その方式を研究する場合は、別のexecution model/versionとして追加する。

## 4. Verified identityが実Packageへ結び付いていること

単にcertificateをwrapperへ添付するのではなく、Packageから再抽出した実artifact digestを含めて`packageBindingIdentity`を計算する。

Package bytes、dispatch、Program、Binding、Shader、Verified operationのどれか一つでも変更すれば、`ValidateFrozenHierarchyLeafV1`は再計算結果との不一致を拒否する。

この検証は「任意HLSLを数学的に証明する」ものではない。Verifierが承認したversioned Program templateと、Target Compilerが生成した実Package artifactを改変不能な対応関係として閉じる。

## 5. Observation Contractの全項目を確認できること

CPU ObserverはA/B/Cについて次の六関係を個別に集計する。

```text
A-reference
B-reference
C-reference
A-B
A-C
B-C
```

各関係で次を記録する。

```text
max absolute error
max relative error
max ULP distance
RMS Euclidean error
```

加えて次を記録する。

```text
first mismatch index
non-finite count
axis-length error
orthogonality error
minimum determinant / orientation
translation error
```

GPU Observer V2はpointごとの304-byte recordに、六関係それぞれのX/Y/Z componentwise absolute error、relative error、ULP distance、およびflags / point indexを書き出す。ULP距離ではIEEE-754の`+0.0f`と`-0.0f`を同一の数値ゼロとして距離0へ正規化し、180度回転などの符号付きゼロ境界でもCPU再構成とGPU観測の意味を一致させる。

`96_Spiral2WarpObservationTests`は全H/F corpus、frame順序入替え、Debug/Release、Recoveryを通して完全なObservation reportを検査する。

## 6. 候補別Materialization / End-to-Endを確認できること

Measurement Evidence schemaはV2である。

旧形式の次の値は削除した。

```text
compositionMaterializationNanoseconds
compositionEndToEndNanoseconds
```

これらはA/B/C合計値であり、候補別数値として読めなかったためである。

V2では次を候補別に保存する。

```text
CandidatePreparationV1.materializationNanoseconds
CandidateTimingSampleV1.endToEndNanoseconds
commandRecordingNanoseconds
gpuTotalNanoseconds
plannedDispatchCount
packageDispatchCount
actual profiled dispatchCount
```

`measurement_samples.csv`の列は次を含む。

```text
candidate_e2e_ns
candidate_materialization_ns
```

Evidence file:

```text
spiral2_measurement_evidence_v2.bin
```

旧V1 evidenceは意味が異なるためV2 Readerでは受理しない。

## 個別の静的監査

ビルド前に、必要な構造が消されていないことだけを確認する場合:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
  -File .\tests\tools\Verify-Spiral2ReviewClosure.ps1
```

この監査は実行結果の代わりではない。最終確認には一括runnerを使用する。

## 成功時の扱い

一括runnerが成功すると、六項目に対する実装と検証環境が成立したことを示す。

ただし、正式なSpiral 2完成宣言にはOwnerによる実行ログの確認、正式Evidenceの選別、Proof Ledger更新、既存CU4以降の再資格、最終Freeze判断が別途必要である。今回のパッチは、そこへ進むための「確認可能な修正版」である。


## Observation Contract V2

Review Closureの成分別評価によって、H07/F10の小さい成分に蓄積したbinary32誤差が正式に観測されるようになった。
V1は履歴として保持し、現在の正本はV2を使用する。V2は相対誤差と剛体性閾値を維持し、成分別評価に必要な絶対下限だけをreference/pairwiseとも`2.5e-4`へ固定する。
詳細は`OBSERVATION_TOLERANCE_IMPLEMENTATION_V2.md`を参照する。
