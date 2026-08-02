# Level 5 垂直実験1 正式結果 — Sparse Indirect／Temporal State／UAV Image

## 1. 結果の位置づけ

本書は`docs/LEVEL5_VERTICAL_EXPERIMENT1_SPARSE_INDIRECT_TEMPORAL_IMAGE.md`で定義した垂直実験1のWindows実行結果を、コード生成物とは独立した正本記録として固定する。

基準となる正本系列は次である。

```text
Level 4 Generalization 7 Full Gate passed
  585737a2ee9d7d5ef6b2abc69f95b224248116a4

Level 5垂直実験1とRunner修正を含む現在地点
  e4bdcf788c5a2fb8eeaabacc9d0c6cfe994ecf5a
```

本実験はProduction ABIを変更していない。対象は`SGE4UNI 2.7`、Schema 17 Leaf、`SGE4INV 1.5`で構成された二つのFrozen Compositionである。

```text
Candidate A = Dense Direct
Candidate B = Verified Sparse Indirect
```

両候補はState Observation、accepted Temporal Aggregate、RGBA32F Texture packed bytes、Controlled Recoveryについて同一の観測結果を満たした後にのみ、State WriterのGPU timestampを比較した。

## 2. Evidence identity

### 実GPU

```text
Device classification     Hardware
Extent                    64 x 64
Universe                  4096
Warmup frames             6
Samples per K             24
Dense Composition         9129241831843b98162196b8ccae6a98a23fc890be640d46a21ff5e912ab398f
Sparse Composition        e0322a773e541dd60d84fbbaf3c549fd1c7dbeed9d44d2a2a9aec1a26587c05a
CSV SHA-256               d3bd553f78bfc11e09f07a4cc17e70cc1113d2e84aeace31bef2087a73768294
Classification            SparseIndirectStableAdvantage
Owner decision            DeferredByOwner
```

### WARP

```text
Device classification     WARP
Extent                    16 x 16
Universe                  256
Warmup frames             2
Samples per K             4
Dense Composition         5d73c7cdf70cfb7dc71b6fcb77eac117e39f91368e8cb358ad6efcff594393ee
Sparse Composition        c800482d526253403494a3c7af6ea3e95bf10d574547afd3c1f1f274b8c9df83
CSV SHA-256               364b83b7591c906cf489fdef14c20c775fa3d353348ddb3155114f6564f4d94e
Classification            SparseIndirectStableAdvantage
Owner decision            DeferredByOwner
```

Evidence CSVはRunnerが`build/evidence/`へ生成する。`build/`はGit管理対象外であるため、本書では再検証に必要なDigestと集計値を正本化する。

## 3. 実GPU結果

| Active K | Active率 | Dense中央値 | Sparse中央値 | Dense / Sparse | Sparse勝 / 同値 / Dense勝 |
|---:|---:|---:|---:|---:|---:|
| 64 | 1.5625% | 304,128 ns | 27,648 ns | 11.000 | 24 / 0 / 0 |
| 256 | 6.25% | 304,128 ns | 37,888 ns | 8.027 | 24 / 0 / 0 |
| 1,024 | 25% | 304,128 ns | 96,256 ns | 3.160 | 24 / 0 / 0 |
| 4,096 | 100% | 305,152 ns | 305,152 ns | 1.000 | 4 / 11 / 9 |

Dense DirectはKに依存せず、Universe 4096件を約304～305 microsecondsで処理した。Verified Sparse IndirectはKに応じて増加し、K=UniverseではDense Directと同一中央値へ収束した。

この形は、候補Bの優位が別Shaderや別の観測意味によるものではなく、Seal済みwork countによって実GPU work量を省略した結果であることと整合する。

## 4. 実行順序の監査

各Kで偶数frameはA→B、奇数frameはB→Aとして、単純な先行実行有利を抑制した。

| K | A→B Dense中央値 | A→B Sparse中央値 | B→A Dense中央値 | B→A Sparse中央値 |
|---:|---:|---:|---:|---:|
| 64 | 303,616 ns | 27,648 ns | 304,128 ns | 27,648 ns |
| 256 | 304,128 ns | 37,888 ns | 304,128 ns | 37,888 ns |
| 1,024 | 304,128 ns | 96,256 ns | 305,152 ns | 96,256 ns |
| 4,096 | 304,128 ns | 305,152 ns | 305,152 ns | 305,152 ns |

K=64、256、1024では、実行順序を反転してもSparse優位が維持された。K=4096では差がtimestamp量子化と実行揺らぎの範囲へ縮退した。

## 5. Command recording監査

実GPUのcommand recording中央値は次である。

| K | Dense recording | Sparse recording |
|---:|---:|---:|
| 64 | 228,250 ns | 233,300 ns |
| 256 | 257,850 ns | 263,350 ns |
| 1,024 | 223,250 ns | 235,500 ns |
| 4,096 | 202,150 ns | 210,800 ns |

Sparse候補のGPU優位はCPU command recordingが軽かったためではない。Sparse側のrecording中央値はむしろ僅かに大きく、観測された主要差はGPUへ接続されたwork量の差である。

## 6. WARP結果

| Active K | Active率 | Dense中央値 | Sparse中央値 | Dense / Sparse | Sparse勝 / 同値 / Dense勝 |
|---:|---:|---:|---:|---:|---:|
| 4 | 1.5625% | 298,550 ns | 29,950 ns | 9.968 | 4 / 0 / 0 |
| 16 | 6.25% | 242,700 ns | 54,000 ns | 4.494 | 4 / 0 / 0 |
| 64 | 25% | 268,300 ns | 114,850 ns | 2.336 | 4 / 0 / 0 |
| 256 | 100% | 317,350 ns | 276,600 ns | 1.147 | 2 / 0 / 2 |

WARPはState／Temporal／Texture観測同値とRecoveryをソフトウェア実装でも成立させた証拠として扱う。sample数が4であるため、100% active時の小さな中央値差をProduction選択根拠には使用しない。

## 7. 成立した縦方向の証拠

両候補について次が同時に成立した。

```text
Verified exact Dynamic payload
  -> Multi-target byte-slice routing
  -> State dense shadow / Texture color shadow
  -> Dense Direct または Verified DispatchIndirect
  -> State Buffer / RGBA32F Texture UAV
  -> Temporal Current / Previous rotation
  -> State Observation / Texture packed readback
  -> Controlled whole-composition Recovery
```

性能sampleを受理する前提として、次を候補間で照合した。

- State Observation Buffer
- successful submit後のaccepted Temporal Aggregate
- RGBA32F Textureのformat、extent、rowBytes、packed bytes digest
- Controlled Recovery後のzero seedからのTemporal再構築
- Dynamic route shadowsとHistoryの原子的Commit

## 8. 確定した結論

本実験が支持する結論は次である。

> New SGE4のFrozen／Verified authorityを維持したまま、exact transition countをD3D12 DispatchIndirectへ接続すると、State、Temporal history、Texture output、Recoveryの観測意味を変えずに、Active率に応じた実GPU work削減が得られる。

本実験ではActive率1.5625%、6.25%、25%でSparseが全24 paired sampleに勝ち、100%では同一中央値へ収束した。

したがって測定分類`SparseIndirectStableAdvantage`は受理する。ただしOwnerによるProduction候補選択は行わず、`DeferredByOwner`を維持する。

## 9. 適用限界

本実験のActive集合は次に限定される。

```text
Active(K) = {0, 1, ..., K-1}
```

Candidate BはDispatch ordinalをそのままmember IDとして使用した。したがって次のような任意疎集合については、本実験は証明を与えない。

```text
{3, 81, 440, 2047}
```

このprefix限定は結果の注記ではなく、次のLevel 4一般化要求である。次段階ではexact Transition setからCanonical compact index listを独立再導出し、Dispatch ordinalからmember IDへの写像をFrozen Dynamic authorityへ含める必要がある。

## 10. 次段階

次の正本能力を`Generalization 8: Verified Compact Sparse Worklist`とする。

```text
Exact Transition Set
  -> canonical ascending uint32 member IDs
  -> independently verified compact worklist
  -> SGE4INV execution-affecting section
  -> fixed-size Dynamic Slot materialization
  -> DispatchIndirect X = worklist count
  -> Shader ordinal -> worklist[ordinal] -> member ID
```

Generalization 8の完成後、Level 5垂直実験2でPrefix、Suffix、Stride、Clustered、fixed-seed Randomなど同一Kの異なるindex分布を比較する。
