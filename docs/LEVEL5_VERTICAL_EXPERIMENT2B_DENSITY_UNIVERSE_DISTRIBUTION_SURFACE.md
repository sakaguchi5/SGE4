# Level 5 垂直実験2b — Density × Universe × Distribution交差面

## 1. 目的

Level 5垂直実験2では、Universe 4096、Active率1.5625%／6.25%／25%で、任意疎Worklistが全分布でDense Directより高速だった。一方、垂直実験1では100% Activeで両候補の中央値が同一へ収束した。

本実験は、その間の交差領域を次の3軸で測定する。

```text
Density
Universe
Distribution
```

Product ABI、SGE4UNI 2.8、SGE4INV 1.6、Schema 17 Leaf、Runtime policyは変更しない。

## 2. 比較候補

垂直実験2と同じ候補を使用する。

```text
Candidate A: Dense Direct + identity index list
Candidate B: Verified Compact Sparse Worklist
```

両候補は同じState Writer Shader、同じ4 Leaf、同じResource graph、同じState／Temporal／Texture観測を持つ。

## 3. 標準交差面

実GPU標準Universe:

```text
1024  = 32 x 32
4096  = 64 x 64
16384 = 128 x 128
```

測定Density:

```text
1/4 = 25.0%
3/8 = 37.5%
1/2 = 50.0%
5/8 = 62.5%
3/4 = 75.0%
7/8 = 87.5%
1/1 = 100.0%
```

25%～87.5%では次の5分布を測る。

```text
Prefix
Suffix
UniformStride
Clustered4
SeededRandom
```

100%では全分布が同じ全集合になるため、重複測定せず`Full`一caseだけを実行する。

したがって各Universeは31 case、標準実GPU全体は93 caseである。

## 4. Warmup境界と測定レジーム資格

初回実行では一部caseのsample列途中に数倍の段階移行が発生し、固定warmup後の全sample中央値が二つの実行レジームを混合した。修正版は二段階のadaptive warmupを使用する。

```text
global warmup per Universe
  minimum 12 paired frames
  maximum 64 paired frames

case-local warmup per Workset
  minimum 16 paired frames
  maximum 64 paired frames

stability window
  latest 8 paired frames
```

windowを前半／後半へ分け、DenseとSparseの双方で対称中央値比が1.05以下になった場合だけ測定へ進む。最大frame数まで収束しない場合は資格失敗とする。

warmup中はState Observation readbackでState Writer completionと候補同値を確認し、full Temporal／Texture readbackはmeasurement sampleで行う。同じD3D12 ExecutorをUniverse内の全caseで再利用し、各caseは新しいComposition Loadから開始する。

16 measurement samplesも前半8／後半8へ分ける。DenseまたはSparseの対称中央値比が1.20を超えたcaseは:

```text
MeasurementRegimeTransition
```

としてRaw Evidenceへ残すが、Surface交差判定から除外する。安定caseは`AcceptedStable`とする。

A-B／B-Aは交互実行する。

## 5. 観測同値

各sampleを受理する前に次をDense／Sparse間比較する。

```text
State Observation
accepted Temporal Aggregate
RGBA32F Texture packed bytes SHA-256
Texture x成分総和
```

各Universeで固定seed乱択50% Worksetを使いControlled Recoveryも確認する。

## 6. Evidence

Raw Evidence:

```text
build/evidence/level5_vertical_v2b_warp.csv
build/evidence/level5_vertical_v2b_hardware_u1024.csv
build/evidence/level5_vertical_v2b_hardware_u4096.csv
build/evidence/level5_vertical_v2b_hardware_u16384.csv
```

Surface Summary:

```text
build/evidence/level5_vertical_v2b_warp_surface.csv
build/evidence/level5_vertical_v2b_hardware_u1024_surface.csv
build/evidence/level5_vertical_v2b_hardware_u4096_surface.csv
build/evidence/level5_vertical_v2b_hardware_u16384_surface.csv
```

Raw CSVはUniverse、Density、Distribution、Workset locality、全timestamp、候補Composition identityを保存する。

Surface CSVはUniverse×Distributionごとに、次を保存する。

```text
最後にSparseが5%以上優位だったDensity
最初にDense/Sparse比が1.05以下となったDensity
交差bracket
100%時のDense/Sparse比
stable／unstable Density点数
series completion
ratioの単調性違反
```

Raw／Surfaceと同じ場所へ`*_timing.csv`も生成し、候補生成、Recovery、global warmup、case load、case warmup、measurement、Evidence write、process totalのwall-clockを保存する。

## 7. 交差判定

各Universe×Distribution系列に、100%の`Full`結果を共通終端として接続する。

```text
Dense/Sparse > 1.05
  = SparseStableAdvantage

0.95 <= Dense/Sparse <= 1.05
  = NoMaterialSeparation

Dense/Sparse < 0.95
  = DenseStableAdvantage
```

`SparseStableAdvantage`の測定点と、それより高密度側の最初の非Sparse点が存在すれば、交差区間をbracketできたとする。

ただし系列内に`MeasurementRegimeTransition`が一つでも存在する場合、その点を飛び越えてbracketを推測しない。

```text
series = MeasurementRegimeIncomplete
overall = CrossoverSurfaceIncomplete
```

全Universe×Distributionで安定したbracketが得られた場合:

```text
CrossoverSurfaceBracketed
```

一部だけなら:

```text
PartialCrossoverSurface
```

測定範囲全体でSparse優位が続いた場合:

```text
SparseDominatesMeasuredSurface
```

最終選択は自動化しない。

```text
OWNER_DECISION = DeferredByOwner
```

## 8. 非目標

```text
RuntimeによるDense／Sparse自動切替
Frozen Variant Set
Worklist生成／Freeze時間を含むend-to-end比較
GPU生成Worklist
別GPU間の一般化
Product ABI変更
```

## 9. 初回探索Evidenceの扱い

修正前の初回CSVは測定レジーム遷移を発見した探索Evidenceとして保持できるが、正式Surface結果には使用しない。修正版Runnerで再測定し、`AcceptedStable`だけから生成されたSurface Summaryを正式正本化する。

## WARPの測定責務

WARPは性能交差面を決定しない。WARP実行では、指定された最小warmup frameを実行した後、全caseについてState／Temporal／Texture観測同値、Compact Worklist、Controlled Recoveryを確認する。timestampとregime ratioは診断Evidenceとして保存するが、5%性能収束やDense／Sparse交差bracketの合否条件にはしない。

WARP Evidenceは次で閉じる。

```text
evidence_role = SemanticQualificationOnly
classification = SemanticQualificationOnly
```

性能交差面の正式判定は実Hardware Evidenceだけが担う。

## 10. Boundary Requalification

完全93 case再実行を避けるため、正式Surface Evidenceは保持したまま、交差境界と既知の不安定地点だけを再資格する短縮Runnerを追加した。

```text
Universe 1024:  7 case
Universe 4096:  8 case
Universe 16384: 7 case
合計:            22 case
```

87.5%の5分布と100% Fullに加え、初回Evidenceでレジーム遷移が見えた4地点をsentinelとして測定する。warmupと12 formal samplesはObservation-only同一経路を使用し、State／Temporal／Texture完全同値は各case開始・終了時に確認する。

測定レジームは前半6／後半6のhalf ratioと、最初4／最後4のedge ratioを同時に検査する。

詳細:

```text
docs/LEVEL5_VERTICAL_EXPERIMENT2B_BOUNDARY_REQUALIFICATION.md
```

実行入口:

```bat
run_sge4_level5_density_universe_distribution_boundary_requalification.bat
```
