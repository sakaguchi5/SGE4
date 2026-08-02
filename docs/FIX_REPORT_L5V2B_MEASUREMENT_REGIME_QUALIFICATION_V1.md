# Level 5 垂直実験2b 修正報告 — Measurement Regime Qualification

## 1. 発見した問題

初回の実GPU交差面測定では、一部caseのsample列の途中でDense／Sparse双方のGPU timestampが数倍へ段階的に移行した。

代表例:

```text
Universe 1024 / Clustered4 / 75%
  前半: Dense 約16～17 us / Sparse 約14～15 us
  後半: Dense 約102～104 us / Sparse 約85～86 us
```

これは単発外れ値ではなく、同一case内に二つの測定レジームが混在した状態である。全sample中央値だけを使うと、定常状態ではSparse優位である点をDense優位または早期交差として誤分類する。

## 2. 修正方針

Product ABI、Frozen Composition、Dynamic Invocation、G8 Compact Worklist、D3D12 Runtimeの意味は変更しない。変更対象は`65_Level5DensityUniverseDistributionExperiment`の測定Harnessだけである。

### 2.1 Adaptive warmup

固定回数warmup後に無条件で測定へ入らない。

```text
minimum warmup
  global: 12 paired frames
  case:   16 paired frames

maximum warmup
  global/case: 64 paired frames

window
  直近8 paired frames
```

直近windowを前半／後半へ分け、DenseとSparseそれぞれについて、

```text
max(firstMedian / lastMedian, lastMedian / firstMedian) <= 1.05
```

となった場合だけ測定へ進む。最大frame数まで収束しなければ資格失敗とする。

warmupではState Observation readbackだけでState Writer completionを確定し、full Temporal／Texture readbackは測定sampleへ残す。これはFrozen意味を変えず、適応warmup追加による同期readback増加を抑えるためである。

### 2.2 Measurement regime判定

16 measurement samplesを前半8／後半8へ分ける。DenseまたはSparseの対称中央値比が1.20を超えた場合:

```text
MeasurementRegimeTransition
```

とする。Raw Evidenceには全sampleと前後半中央値を保存するが、Surface交差判定からは除外する。

安定caseは:

```text
AcceptedStable
```

とする。

### 2.3 Surfaceの保守的判定

Universe×Distribution系列に一つでも不安定なDensity点がある場合、欠落点を飛び越えて交差bracketを推測しない。

```text
series status = MeasurementRegimeIncomplete
overall classification = CrossoverSurfaceIncomplete
```

全Density点が`AcceptedStable`の場合だけ従来の交差bracketを生成する。

### 2.4 Timing Evidence

Raw／Surface Evidenceに加え、次を`*_timing.csv`へ保存する。

```text
candidate build
Controlled Recovery
global warmup
case Composition load合計
case adaptive warmup合計
measurement合計
Evidence write
process total
```

各Raw caseにもload／warmup／measurement秒数を保存する。

## 3. 初回Evidenceへの回帰適用

閾値1.20を初回実GPUCSVへ適用すると、次の5 caseが正しく検出される。

```text
Universe 1024
  Clustered4 75%
  Prefix 87.5%

Universe 16384
  UniformStride 25%
  UniformStride 50%
  Full 100%
```

Universe 4096では不安定caseを検出しない。

この判定により、Universe 1024の偽の早期交差bracket、およびUniverse 16384の混合Full中央値を正式Surfaceへ混入させない。

## 4. 非変更範囲

```text
SGE4UNI 2.8
SGE4INV 1.6
Schema 17 Leaf
Compact Worklist authority
Dense／Sparse候補Composition
State Writer Shader
Density／Universe／Distribution測定点
Controlled Recovery契約
```

は変更していない。
