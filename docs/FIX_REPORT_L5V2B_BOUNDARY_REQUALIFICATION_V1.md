# Fix Report — Level 5垂直実験2b Boundary Requalification

## 問題

完全交差面は全Universe／全Distributionで`[87.5%, 100%]`を示したが、一部caseではObservation-only warmupからFull readback measurementへ切り替えた直後に、最初の2～3 sampleだけ異なるGPU timestampレジームが残った。前半8／後半8の中央値比較では、この短い立ち上がりが中央値へ隠れる場合があった。

## 修正

- 完全93 case Runnerは保持する。
- 22 caseのBoundary Requalification modeを追加する。
- warmupとformal measurementをObservation-only同一経路に固定する。
- State／Temporal／Texture完全同値はcase開始・終了時に確認する。
- measurementは12 paired samplesとする。
- 前半6／後半6だけでなく、最初4／最後4のedge ratioも検査する。
- 最新Raw Evidenceのedge ratioで検出したU4096 Suffix 25%／SeededRandom 50%、U16384 UniformStride 37.5%をsentinelとして再測定する。
- 以前の探索runで遷移したU1024 Clustered4 75%もhistorical sentinelとして保持する。
- 87.5%全5分布と100% Fullから境界を再資格する。

## 非変更

```text
SGE4UNI 2.8
SGE4INV 1.6
Schema 17 Leaf
Verified Compact Sparse Worklist authority
D3D12 Runtime
Dense／Sparse Candidate Composition
既存93 case Evidence
```

## 期待する短縮

完全交差面は93 case、16 formal samples、毎sample Full readbackだった。Boundary Requalificationは22 case、12 formal samples、Full readbackは各case開始・終了時だけである。意味資格を維持しつつ、再実行対象と同期readbackを大幅に削減する。
