# Level 5 垂直実験2b 正式結果 — Density × Universe × Distribution Crossover Surface

## 1. 正本化の対象

本書は、`docs/LEVEL5_VERTICAL_EXPERIMENT2B_DENSITY_UNIVERSE_DISTRIBUTION_SURFACE.md`および
`docs/LEVEL5_VERTICAL_EXPERIMENT2B_BOUNDARY_REQUALIFICATION.md`で定義したLevel 5垂直実験2bの
WARP意味資格、実GPU完全交差面、Boundary Requalification結果を正式証拠として固定する。

基準実装:

```text
commit = 9d299b3cd3cd03e043f396f66e7adbbf319e445b
Frozen Composition = SGE4UNI 2.8
Frozen Dynamic Invocation = SGE4INV 1.6
Leaf Package = Schema 17
```

比較候補:

```text
Candidate A = Dense Direct + identity index list
Candidate B = Verified Compact Sparse Worklist
```

両候補は同じSchema 17 Leaf集合、同じResource graph、同じState Writer Shader、
同じState／Temporal／RGBA32F Texture観測契約を使用する。
差は、DenseがUniverse全体を実行し、Sparseが独立検証済みCompact Worklistの件数だけ
`DispatchIndirect`する点である。

Product ABI、Frozen authority、Runtime policyは本実験結果によって変更しない。
Owner decisionは引き続き次とする。

```text
OWNER_DECISION = DeferredByOwner
```

## 2. Evidence chain

受領した原本Archive:

```text
file = 結果まとめ.zip
sha256 = df88025a044f0f415479ce91acd29e1fe7b7b8ee5cae357aad6550fadda64f30
contained CSV files = 21
```

Raw CSVは`build/evidence/`から生成された実行生成物であり、Source treeの正本へ直接含めない。
従来の正式結果文書と同様に、本書へEvidence identity、SHA-256、集計値、成立範囲、非成立範囲を固定する。
21個のCSVそのものは、別途`SGE4_L5V2B_Evidence_Snapshot_20260802.zip`として同一bytesのArchiveを作成する。

## 3. WARP意味資格

WARP Evidenceは性能交差を決定する証拠ではなく、次の意味契約をソフトウェア実装上でも資格化する。

```text
Verified Compact Sparse Worklist
State Observation
accepted Temporal Aggregate
RGBA32F Texture packed bytes
Controlled whole-composition Recovery
```

結果:

```text
Universe = 256
Extent = 16 x 16
Cases = 31
Samples per case = 8 paired samples
Accepted = 31
Unstable = 0
Classification = SemanticQualificationOnly
```

WARPのglobal warmup timestamp比はDenseで4.53512、Sparseで1.06126だったが、
WARPでは性能収束を合否条件にせず、全caseの観測同値を確認した。
WARP timestampをProduction候補選択または交差境界の根拠には使用しない。

## 4. 実GPU完全交差面

測定範囲:

```text
Universe = 1024, 4096, 16384
Density = 25%, 37.5%, 50%, 62.5%, 75%, 87.5%, 100%
Distribution = Prefix, Suffix, UniformStride, Clustered4, SeededRandom
100% = Full case
Cases = 31 per Universe, 93 total
Samples = 16 paired samples per case, 1488 total
```

全93 caseが`AcceptedStable`となり、`MeasurementRegimeTransition`は0件だった。
3 Universe × 5 Distributionの全15系列で、Surfaceは次を記録した。

```text
Sparse優位の最終点 = 87.5%
Dense/Sparse同等点 = 100%
Crossover bracket = [87.5%, 100%]
Series complete = true
Monotonicity violation = false
Classification = CrossoverSurfaceBracketed
```

### Dense/Sparse比

| Universe | Active率 | Dense/Sparse 最小 | Dense/Sparse 最大 | 対象 |
|---:|---:|---:|---:|---|
| 1,024 | 25% | 2.198 | 2.405 | 5 distributions |
| 1,024 | 37.5% | 1.754 | 1.887 | 5 distributions |
| 1,024 | 50% | 1.449 | 1.608 | 5 distributions |
| 1,024 | 62.5% | 1.342 | 1.389 | 5 distributions |
| 1,024 | 75% | 1.190 | 1.220 | 5 distributions |
| 1,024 | 87.5% | 1.099 | 1.099 | 5 distributions |
| 1,024 | 100% | 1.005 | 1.005 | Full |
| 4,096 | 25% | 2.910 | 3.075 | 5 distributions |
| 4,096 | 37.5% | 2.267 | 2.352 | 5 distributions |
| 4,096 | 50% | 1.785 | 1.817 | 5 distributions |
| 4,096 | 62.5% | 1.515 | 1.543 | 5 distributions |
| 4,096 | 75% | 1.286 | 1.293 | 5 distributions |
| 4,096 | 87.5% | 1.138 | 1.146 | 5 distributions |
| 4,096 | 100% | 1.000 | 1.000 | Full |
| 16,384 | 25% | 3.647 | 3.715 | 5 distributions |
| 16,384 | 37.5% | 2.544 | 2.563 | 5 distributions |
| 16,384 | 50% | 1.944 | 1.959 | 5 distributions |
| 16,384 | 62.5% | 1.567 | 1.569 | 5 distributions |
| 16,384 | 75% | 1.317 | 1.319 | 5 distributions |
| 16,384 | 87.5% | 1.136 | 1.138 | 5 distributions |
| 16,384 | 100% | 1.000 | 1.000 | Full |

`Dense/Sparse > 1`はSparseのGPU中央値が小さいことを表す。

重要な特徴は次の三点である。

1. 25%から87.5%まで、全Universe・全Distributionのcase中央値でSparseが優位だった。
2. Universeが大きいほど、低DensityでのSparse優位が大きくなった。
3. 100% FullではU=4096とU=16384が完全同一中央値、U=1024も1.005で同等帯へ収束した。

### paired sample監査

```text
全sample:
  Sparse勝利 = 1457
  同値 = 17
  Dense勝利 = 14
  合計 = 1488

100%未満:
  Sparse勝利 = 1438
  同値 = 0
  Dense勝利 = 2
  合計 = 1440
```

100%未満の90 caseはすべてcase中央値でSparse優位だった。
個別sampleの反転は1440中2件だけであり、系列中央値または交差判定を変えなかった。

## 5. DistributionとUniverseの意味

分布差は存在するが、今回の交差境界をDistributionごとに分離するほど大きくなかった。

- U=1024ではUniformStrideが低～中Densityで相対的に遅く、Clustered4が比較的有利だった。
- U=4096でもUniformStrideが多くのDensityで最小比となったが、87.5%では全分布が1.138～1.146へ集中した。
- U=16384では25%で3.647～3.715、87.5%で1.136～1.138となり、分布spreadはUniverse増加とともに交差境界付近でさらに小さくなった。

したがって、この実GPU・この候補構造・このDensity格子では、
交差を支配した第一変数はDistributionではなくActive率であり、
Universeは低Density側のSparse優位幅を増幅した。

ただし本実験は、真の連続交差点が87.5%と100%のどこにあるかまでは決定しない。

## 6. Boundary Requalification

完全93 case Evidenceを保持したまま、次をObservation-only同一測定経路で再資格した。

```text
87.5%: 全Universe × 全5 Distribution
100%: 全Universe × Full
Sentinel:
  U=1024 Clustered4 75%
  U=4096 Suffix 25%
  U=4096 SeededRandom 50%
  U=16384 UniformStride 37.5%
Total cases = 22
Samples = 12 paired samples per case
```

結果:

```text
AcceptedStable = 22
MeasurementRegimeTransition = 0
Boundary series requalified = 15 / 15
Classification = BoundaryCrossoverRequalified
```

全15境界系列で、87.5%は`Dense/Sparse > 1.05`、100% Fullは`0.95 <= Dense/Sparse <= 1.05`を満たした。

### Sentinel再資格

| Universe | Sentinel | Dense中央値 | Sparse中央値 | Dense/Sparse | half ratio (D/S) | edge ratio (D/S) |
|---:|---|---:|---:|---:|---:|---:|
| 1,024 | Clustered4 75% | 101,376 ns | 84,992 ns | 1.193 | 1.005/1.000 | 1.005/1.000 |
| 4,096 | Suffix 25% | 313,344 ns | 102,400 ns | 3.060 | 1.005/1.000 | 1.003/1.000 |
| 4,096 | SeededRandom 50% | 313,856 ns | 175,104 ns | 1.792 | 1.003/1.003 | 1.002/1.000 |
| 16,384 | UniformStride 37.5% | 1,162,240 ns | 456,704 ns | 2.545 | 1.000/1.001 | 1.000/1.001 |

half ratioとedge ratioはいずれも1.20を大きく下回った。
以前の探索runまたは最新Raw edge監査で注目されたcaseは、同一測定経路ではすべて安定し、
Sparse優位を再確認した。

## 7. 実行時間

| Evidence | Cases | Accepted | Unstable | Total | Case warmup | Measurement |
|---|---:|---:|---:|---:|---:|---:|
| Hardware U=1,024 | 31 | 31 | 0 | 13分21.3秒 | 4分48.7秒 | 7分21.7秒 |
| Hardware U=4,096 | 31 | 31 | 0 | 13分48.6秒 | 4分54.2秒 | 7分31.0秒 |
| Hardware U=16,384 | 31 | 31 | 0 | 15分34.1秒 | 5分59.9秒 | 8分15.9秒 |
| Boundary U=1,024 | 7 | 7 | 0 | 1分50.5秒 | 0分45.4秒 | 0分37.7秒 |
| Boundary U=4,096 | 8 | 8 | 0 | 2分31.2秒 | 0分59.5秒 | 0分56.2秒 |
| Boundary U=16,384 | 7 | 7 | 0 | 2分44.4秒 | 0分58.7秒 | 1分7.3秒 |
| WARP semantic | 31 | 31 | 0 | 0分6.9秒 | 0分2.2秒 | 0分2.6秒 |

完全実GPU交差面3本の合計は`42分44.0秒`、
Boundary Requalification 3本の合計は`7分6.0秒`、
WARPは`0分6.9秒`だった。

完全交差面の合計時間内訳:

```text
case adaptive warmup = 942.814 s (36.8%)
formal measurement = 1388.578 s (54.2%)
case load = 143.703 s (5.6%)
global warmup = 36.577 s (1.4%)
recovery = 12.765 s (0.5%)
```

時間の約90.9%はcase-local warmupとformal measurementである。
ファイル書き出しやCandidate buildが遅さの原因ではない。
長時間になった主因は、93 caseそれぞれで測定レジームを資格化し、
16 paired sampleとFull State／Temporal／Texture readbackを繰り返したことにある。

Boundary Requalificationは22 caseへ限定し、Full readbackをcase開始・終了へ集約したことで、
完全交差面3本の約16.6%の時間で境界を再資格した。

## 8. 確定した結論

本実験が支持する主張は次である。

> Verified Compact Sparse Worklistは、Universe 1024／4096／16384、
> Prefix／Suffix／UniformStride／Clustered4／固定seed乱択の全測定系列で、
> State、Temporal、RGBA32F Texture、Recoveryの意味を維持したまま、
> 87.5% ActiveまでDense Directより小さい実GPU中央値を示し、
> 100% ActiveでDense Directと同等へ収束した。

さらに、測定レジーム資格後の完全93 caseと、
同一測定経路による22 caseのBoundary Requalificationが同じ境界結論を支持した。

正式分類:

```text
Performance Surface = CrossoverSurfaceBracketed
Boundary = BoundaryCrossoverRequalified
WARP = SemanticQualificationOnly
Owner decision = DeferredByOwner
```

## 9. 確定しないこと

本実験だけでは次を確定しない。

```text
87.5%～100%の連続的な真の交差密度
別GPU／別Driverでの交差位置
Worklist生成、Freeze、uploadを含むend-to-end損益
毎frame大きく変化するWorklistの再構築費用
Distributionがより悪条件になった場合の上限
Runtimeによる自動候補選択
Production policy
```

したがって、`Sparseを常に選択する`、`87.5%を固定閾値にする`、
`HardwareごとのRuntime分岐を追加する`といった方針変更は、この結果文書だけからは行わない。

## 10. Evidence SHA-256

| File | SHA-256 | Classification |
|---|---|---|
| `level5_vertical_v2b_warp.csv` | `c64972afaba4ee77417ddd3d3ec7f17d08a9abbf4e73d255d2e36ac09d95b0a3` | `SemanticQualificationOnly` |
| `level5_vertical_v2b_warp_surface.csv` | `63c7690eda58c9f19e7dd5ac82a01229af8ab6d7c8f226a4314f7f4be6d13cb1` | `SemanticQualificationOnly` |
| `level5_vertical_v2b_warp_timing.csv` | `36f85fd40e6081e848343fc0cce06238ab6bf161beb01e5c2a7ccb924afc3d3c` | `SemanticQualificationOnly` |
| `level5_vertical_v2b_hardware_u1024.csv` | `ff28cbe2624de78e21216de5cffbc701ec6c7d27cdd03d7ae2388c827bc15a2f` | `CrossoverSurfaceBracketed` |
| `level5_vertical_v2b_hardware_u1024_surface.csv` | `7147e3c51c315645e19d4ba2e21d9726dd3d49d9afab4c1fa8d99af97c5796f0` | `CrossoverSurfaceBracketed` |
| `level5_vertical_v2b_hardware_u1024_timing.csv` | `a10d3322247031903493f0e14fdaef0ccede672e8ba85ea26b4176a862e6b87f` | `CrossoverSurfaceBracketed` |
| `level5_vertical_v2b_hardware_u4096.csv` | `5705d8ba2f63b6cd3582e6e31c1ab1b2e36b9bba11435069860f6da139454f7a` | `CrossoverSurfaceBracketed` |
| `level5_vertical_v2b_hardware_u4096_surface.csv` | `bb3c290ebe15e785efd0b18ae915d1c0bf0f556f6bd9365edd081c92ce4028d9` | `CrossoverSurfaceBracketed` |
| `level5_vertical_v2b_hardware_u4096_timing.csv` | `99556f672ee42ed73998d4064216daf8d1161c8cdc8c57fa021ed3ca714a2bbe` | `CrossoverSurfaceBracketed` |
| `level5_vertical_v2b_hardware_u16384.csv` | `60efee7d22acbef934c9df2856209417dea961623a58f5a9887290f1394f4cea` | `CrossoverSurfaceBracketed` |
| `level5_vertical_v2b_hardware_u16384_surface.csv` | `b21d3fe97b6d1f75a1be003d9b21609716314ed69d2dd93238649c1e11166a28` | `CrossoverSurfaceBracketed` |
| `level5_vertical_v2b_hardware_u16384_timing.csv` | `eb7d24850cdd2b6d9b0fb74c005ec1802993fc128313e9c89cee79652afe7c9a` | `CrossoverSurfaceBracketed` |
| `level5_vertical_v2b_boundary_requalification_u1024.csv` | `9527d81808c4c8b87df91b7432e3ded28bc61e747bf7ed929b2c57b9181092ee` | `BoundaryCrossoverRequalified` |
| `level5_vertical_v2b_boundary_requalification_u1024_surface.csv` | `2946c371460caaf34df81639d2ae4fee38a75fd69837e8bf02679a53993095a4` | `BoundaryCrossoverRequalified` |
| `level5_vertical_v2b_boundary_requalification_u1024_timing.csv` | `8b2214071cd6d98c1bbc3924c98fe0297ce3968583461cc3bc9d67dc9a78d701` | `BoundaryCrossoverRequalified` |
| `level5_vertical_v2b_boundary_requalification_u4096.csv` | `72dfb0ebfbcecada74f97e6aa024291423c4ea5c2dade1e3121d3252bd0ed28e` | `BoundaryCrossoverRequalified` |
| `level5_vertical_v2b_boundary_requalification_u4096_surface.csv` | `a0bc9e332c7cda0f4ee8be9b898ab5e1188bcaa83bc5b0bd4535254e56bb2df6` | `BoundaryCrossoverRequalified` |
| `level5_vertical_v2b_boundary_requalification_u4096_timing.csv` | `e760ebfbdbfce5967f4149f56a4da4ba10676ee3cd06e2706525e6fdb9d63df9` | `BoundaryCrossoverRequalified` |
| `level5_vertical_v2b_boundary_requalification_u16384.csv` | `e65b17c1e368e97f6c56e30cc24dd5bea2265c98ab4dddd07839272b36a669ca` | `BoundaryCrossoverRequalified` |
| `level5_vertical_v2b_boundary_requalification_u16384_surface.csv` | `856d0837b62fbe1c0dc5e027b11698d655fada809fa46bf48d94ae68ed1cc64b` | `BoundaryCrossoverRequalified` |
| `level5_vertical_v2b_boundary_requalification_u16384_timing.csv` | `90affdf4a2bae92a896d02428d41c9cb89382b42bfa8b7640b7458d4cda1d661` | `BoundaryCrossoverRequalified` |
