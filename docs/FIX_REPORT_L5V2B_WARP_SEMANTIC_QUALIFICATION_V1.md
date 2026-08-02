# Level 5 垂直実験2b 修正報告 — WARP Semantic Qualification

## 1. 発見した問題

測定レジーム資格修正後、WARP quick資格が次で停止した。

```text
Universe専用global warmupが最大16 frame以内に均質な測定レジームへ収束しませんでした。
denseRatio=1.077101
sparseRatio=1.524954
```

5%収束条件は、実GPUの性能交差面を均質なtimestamp regimeから作るための条件である。一方WARPはGPU性能候補を選ぶ証拠ではなく、State／Temporal／Texture観測同値、Compact Worklist、Controlled Recoveryをソフトウェア実装で資格化するための実行である。WARP timestampへ実GPUと同じ性能収束条件を課したことが責務の混同だった。

## 2. 修正

`--warp`実行では次のように分離する。

```text
warmup
  最小指定frame数を実行
  Dense／Sparseのtimestamp前後半比はEvidenceへ保存
  5%性能収束を合否条件にしない

measurement
  全State／Temporal／RGBA32F Texture同値を確認
  timestamp sampleはRaw Evidenceへ保存
  performance crossover判定には使用しない

classification
  SemanticQualificationOnly
```

実Hardwareでは従来どおり、5% adaptive warmup収束と20% measurement regime transition拒否を維持する。

## 3. Evidence

WARP Raw／Surface／Timing CSVへ次を明記する。

```text
evidence_role=SemanticQualificationOnly
classification=SemanticQualificationOnly
measurement_status=SemanticQualificationOnly
```

Surface CSVは交差bracketを生成せず、各Distributionを`SemanticQualificationOnly`として記録する。WARP timestampは診断用に残るが、Dense／SparseのProduction候補選択根拠にはしない。

## 4. 非変更範囲

```text
SGE4UNI 2.8
SGE4INV 1.6
Schema 17 Leaf
G8 Compact Worklist authority
Dense／Sparse Composition
実Hardwareの収束閾値
実Hardwareのmeasurement regime判定
Density／Universe／Distribution測定点
```

は変更していない。
