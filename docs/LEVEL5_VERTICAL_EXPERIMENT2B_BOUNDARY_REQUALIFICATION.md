# Level 5 垂直実験2b — Boundary Requalification

## 1. 位置づけ

完全交差面Runnerは、Universe 1024／4096／16384、各31 case、合計93 caseを測定し、全Universe／全Distributionで次の交差区間を得た。

```text
Sparse優位の最終測定点: 87.5%
Dense／Sparse同等点:     100%
Crossover bracket:       [87.5%, 100%]
```

一方、完全交差面の一部sampleでは、Observation-only warmupからFull Temporal／Texture readbackを伴うmeasurementへ切り替えた直後に立ち上がりレジームが残った。全93 caseを再測定せず、既存Evidenceを保持したまま、交差境界と既知の不安定点だけを同一測定経路で再資格する。

Product ABI、SGE4UNI 2.8、SGE4INV 1.6、Schema 17 Leaf、G8 authority、Runtime policyは変更しない。

## 2. 測定case

各Universeで87.5%の5分布と100% Fullを測定する。

```text
87.5%:
  Prefix
  Suffix
  UniformStride
  Clustered4
  SeededRandom

100%:
  Full
```

最新の完全交差面Raw Evidenceへedge ratio判定を適用すると、Universe 4096のSuffix 25%／SeededRandom 50%、Universe 16384のUniformStride 37.5%で短い立ち上がりを検出した。これらに、以前の探索runで遷移が見えたUniverse 1024のClustered4 75%を加え、sentinelとして再資格する。

```text
Universe 1024:
  Clustered4 75%
  境界6 case
  合計7 case

Universe 4096:
  Suffix 25%
  SeededRandom 50%
  境界6 case
  合計8 case

Universe 16384:
  UniformStride 37.5%
  境界6 case
  合計7 case
```

全体は22 caseである。既存93 case Raw／Surface Evidenceは削除・置換しない。

## 3. 同一測定経路

各caseを次の順序で実行する。

```text
1. Full start qualification
   State Observation
   accepted Temporal Aggregate
   RGBA32F Texture packed bytes SHA-256
   Texture x成分総和

2. Observation-only adaptive warmup
   minimum 8 paired frames
   maximum 32 paired frames
   latest 8 frameの前半／後半中央値比 <= 1.05

3. Observation-only formal measurement
   12 paired samples
   A-B／B-Aを交互実行

4. Full end qualification
   State／Temporal／Textureの完全同値
   successful submit後のTemporal rotation
```

warmupと正式measurementは同じObservation-only completion pathを使用する。Full readbackはcase開始時と終了時だけ実行し、意味同値を保持しながら同期readback回数を削減する。

## 4. 測定レジーム判定

12 samplesについて、二つの判定を同時に行う。

```text
half ratio:
  前半6 sample中央値
  後半6 sample中央値

edge ratio:
  最初4 sample中央値
  最後4 sample中央値
```

DenseまたはSparseのhalf ratio／edge ratioのいずれかが1.20を超えた場合:

```text
MeasurementRegimeTransition
```

とし、Boundary bracketへ使用しない。

## 5. Boundary判定

各Distributionについて次を要求する。

```text
87.5%:
  AcceptedStable
  Dense/Sparse > 1.05

100% Full:
  AcceptedStable
  0.95 <= Dense/Sparse <= 1.05
```

sentinelもすべて`AcceptedStable`かつSparse優位でなければならない。

全条件を満たした場合:

```text
BoundaryCrossoverRequalified
```

不安定caseがある場合:

```text
BoundaryRequalificationIncomplete
```

安定しているが従来の境界結論と一致しない場合:

```text
BoundaryResultChanged
```

Owner decisionは自動化しない。

```text
OWNER_DECISION = DeferredByOwner
```

## 6. Evidence

```text
build/evidence/level5_vertical_v2b_boundary_requalification_u1024.csv
build/evidence/level5_vertical_v2b_boundary_requalification_u4096.csv
build/evidence/level5_vertical_v2b_boundary_requalification_u16384.csv
```

各Raw Evidenceと同じ場所へ`_surface.csv`と`_timing.csv`を生成する。

Raw CSVは従来のhalf ratioに加えて、leading／trailing中央値とedge ratio、Full start／end qualification時間を保存する。

## 7. 実行入口

```bat
run_sge4_level5_density_universe_distribution_boundary_requalification.bat
```

完全93 case Runnerは比較・再現用として保持する。

```bat
run_sge4_level5_density_universe_distribution_experiment.bat
```
