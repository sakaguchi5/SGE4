# Level 5 垂直実験2 正式結果 — Arbitrary Sparse Worklist

## 1. 正本化の対象

本書は、Level 4 Generalization 8 `Verified Compact Sparse Worklist`を使用したLevel 5垂直実験2のWindows／WARP／実GPU結果を正式証拠として固定する。

基準実装:

```text
commit = e92379e82b222fdec11451a265c553d94eb7dede
Frozen Composition = SGE4UNI 2.8
Frozen Dynamic Invocation = SGE4INV 1.6
Leaf Package = Schema 17
```

比較候補:

```text
Candidate A = Dense Direct + identity index list
Candidate B = Verified Compact Sparse Worklist
```

両候補は同じ4個のSchema 17 Leaf Package、同じComposition Contract、同じResource graph、同じState Writer Shaderを使用する。差は、DenseがUniverse全体をDirect Dispatchするのに対し、Sparseが独立検証済みCompact Worklistの件数だけDispatchIndirectする点である。

## 2. Evidence identity

### 実GPU

```text
file = level5_vertical_v2_hardware.csv
sha256 = acdea914c8a6f19f5642183c6bd7f761826adb35d342ff5ba9c754acfda7a797
device = Hardware
extent = 64 x 64
universe = 4096
warmup frames per case = 6
sample frames per case = 24
random seed scheme = L5VERT21-xorshift64star-universe-active
dense composition = bbcb9ea67c6b4c2e5f8c034ff5545f9ab76e6d82d7dca818c6f62638bc4ae39e
sparse composition = 6c537ca8e38d0c45d21f41edb15a4a8efe25f1b6f1577a241ee7c92f5c76d48c
classification = ArbitrarySparseAdvantageDistributionStable
owner decision = DeferredByOwner
```

### WARP

```text
file = level5_vertical_v2_warp.csv
sha256 = 43ecc89c8a8df33338dc7d2a5f414c08cc11fbf158418e9c41daffaffa02a8a1
device = WARP
extent = 16 x 16
universe = 256
warmup frames per case = 2
sample frames per case = 4
random seed scheme = L5VERT21-xorshift64star-universe-active
dense composition = dedefe5eb2b612a0cfb10e63c5e559d78bd5bd053dd21fef6381bba04e817956
sparse composition = 11c61d5b8cd907b817e2a4170497df07c7cb7757e3c11204ec9e152e6aaa5a6e
classification = ArbitrarySparseAdvantageDistributionSensitive
owner decision = DeferredByOwner
```

生CSVは`build/evidence/`の生成物でありGit正本へ含めない。本書はEvidenceのidentity、集計結果、成立範囲、非成立範囲を正本化する。

## 3. 観測同値と回復

全caseで性能値を受理する前に、Dense／Sparse間の次を比較した。

```text
State Observation
accepted Temporal Aggregate
RGBA32F Texture packed bytes SHA-256
Texture x成分総和
```

さらに固定seed乱択Worksetを用いてControlled whole-composition Recoveryを実行し、次を確認した。

```text
Device epoch更新
External rebind acknowledgement
旧Compact Worklist binding失効
旧Temporal history失効
RecoverySeedからの任意疎Worklist再構築
Previous Temporalの明示zero seed復帰
State／Temporal／Texture観測同値
```

したがって以下の性能結果は、候補間の意味差、Temporal差、Texture差、Recovery差を含まないsampleだけから構成される。

## 4. 実GPU結果

単位はnanoseconds。勝敗は各case 24個のpaired sampleについて`Sparse < Dense`を勝ちとした。

| Active K | Active率 | Distribution | runs | Dense中央値 | Sparse中央値 | Dense/Sparse | Sparse勝利 |
|---:|---:|---|---:|---:|---:|---:|---:|
| 64 | 1.5625% | Prefix | 1 | 312832 | 32768 | 9.547 | 24/24 |
| 64 | 1.5625% | Suffix | 1 | 314368 | 32768 | 9.594 | 24/24 |
| 64 | 1.5625% | UniformStride | 64 | 313344 | 32768 | 9.562 | 24/24 |
| 64 | 1.5625% | Clustered4 | 4 | 313344 | 32768 | 9.562 | 24/24 |
| 64 | 1.5625% | SeededRandom | 61 | 314368 | 32768 | 9.594 | 24/24 |
| 256 | 6.25% | Prefix | 1 | 314368 | 43008 | 7.310 | 24/24 |
| 256 | 6.25% | Suffix | 1 | 313344 | 43008 | 7.286 | 24/24 |
| 256 | 6.25% | UniformStride | 256 | 314368 | 46080 | 6.822 | 24/24 |
| 256 | 6.25% | Clustered4 | 4 | 313344 | 43008 | 7.286 | 24/24 |
| 256 | 6.25% | SeededRandom | 242 | 314368 | 46080 | 6.822 | 24/24 |
| 1024 | 25% | Prefix | 1 | 313344 | 102400 | 3.060 | 24/24 |
| 1024 | 25% | Suffix | 1 | 313344 | 101888 | 3.075 | 24/24 |
| 1024 | 25% | UniformStride | 1024 | 314368 | 108544 | 2.896 | 24/24 |
| 1024 | 25% | Clustered4 | 4 | 313856 | 102400 | 3.065 | 24/24 |
| 1024 | 25% | SeededRandom | 774 | 314368 | 103424 | 3.040 | 24/24 |

全15 case、全360 paired sampleの結果:

```text
Sparse勝利 = 360
同値 = 0
Dense勝利 = 0
```

## 5. 分布感度

同じKにおけるSparse中央値の最大／最小は次である。

| K | 最小Sparse | 最大Sparse | 最大/最小 | Dense/Sparse範囲 |
|---:|---:|---:|---:|---:|
| 64 | 32768 | 32768 | 1.0000 | 9.547～9.594 |
| 256 | 43008 | 46080 | 1.0714 | 6.822～7.310 |
| 1024 | 101888 | 108544 | 1.0653 | 2.896～3.075 |

実GPUでは分布によるspreadは最大7.14%であり、事前規則の10%閾値を超えなかった。よって正式分類は次である。

```text
ArbitrarySparseAdvantageDistributionStable
```

これは分布差がzeroであることを意味しない。UniformStride／SeededRandomはK=256およびK=1024で連続分布よりやや遅い。ただし主要変数はKであり、今回の測定範囲では局所性は二次的要因だった。

Clustered4は4個の分離領域を持つが、各cluster内部の連続性によりPrefix／Suffixとほぼ同じ中央値だった。

## 6. 実行順とrecording境界

各caseはA-B／B-Aを12回ずつ交互実行した。K=64の最初のPrefix caseではGPU安定化途中のsampleが残ったが、他のcaseを含む全360 paired sampleでSparseが勝利し、case中央値も安定領域を捉えた。

command-recording中央値の範囲:

```text
Dense = 205900～231250 ns
Sparse = 210400～231800 ns
```

SparseがCPU recordingで一貫して有利という結果ではない。したがってGPU timestamp差の主因は、CPU command生成ではなく実行member数の削減である。

## 7. WARPの位置づけ

WARPでも全60 paired sampleでSparseが勝利した。ただし各case 4 sampleであり、実行順による揺らぎが実GPUより大きい。WARPの`DistributionSensitive`分類は性能一般化には使用しない。

WARPの正本上の役割は次である。

```text
任意WorklistのCanonical生成
SGE4INV 1.6 Freeze
fixed-size Slot materialization
State／Temporal／Texture観測同値
Controlled Recovery
```

性能判断の主証拠は24 sample × 15 caseの実GPU結果とする。

## 8. 確定した主張

本実験により、次を実証した。

> New SGE4は、exact Transition setから独立検証された任意のCanonical Compact Worklistを生成し、Dispatch ordinalを非prefix member IDへ接続できる。Prefix、Suffix、UniformStride、Clustered4、固定seed乱択の全分布でDense実行と同じState／Temporal／Texture／Recovery意味を維持しながら、Active件数に応じた実GPU時間削減を得た。

今回のUniverse 4096では、Sparse優位は次の範囲だった。

```text
Active 1.5625% = 約9.55～9.59倍
Active 6.25%   = 約6.82～7.31倍
Active 25%     = 約2.90～3.08倍
```

## 9. 適用限界

本実験だけでは次を確定しない。

```text
Active率25%～100%の交差位置
Universeサイズによる交差位置の変化
別GPU architectureでの交差位置
GPU生成Worklist
Worklist生成／Freeze／uploadを含むend-to-end時間
毎frame大きく変化するWorklistの費用
Runtimeによる候補自動選択
```

したがってOwner Decisionは維持する。

```text
OWNER_DECISION = DeferredByOwner
```

次の直接実験は`Density × Universe × Distribution`交差面であり、Dense／Sparseが同等へ収束する密度区間をUniverse別・分布別に測定する。
