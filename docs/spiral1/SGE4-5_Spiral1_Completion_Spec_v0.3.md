# Semantic GPU Engine 4-5

# Spiral 1：PGA剛体変換表現保持実験 完成仕様 v0.3

- 状態: Frozen Completion Specification
- 日付: 2026-07-19
- Foundation commit: `2e79dbf96255f1cac4dbb9fc133662123e78cf4a`
- 固定基準: SGE4 Level 4 v1 canonical archive `SGE4v1.zip` / SHA-256 `52e0567bbd86cef2a1840e09d3a9f0d5efc128b4170ceb495a4905ab6d659894`
- 研究方針: Level 4とLevel 5を相互に導く螺旋型の研究

---

## Contract Freeze declaration

このv0.3は、SGE4-5 Stage 03で固定されたSpiral 1の正本仕様である。

次の分冊契約と`CONTRACT_MANIFEST_V1.json`を合わせて一つのResearch Contractとする。

```text
SEMANTIC_CONVENTION_V1.md
OBSERVATION_CONTRACT_V1.md
CORPUS_V1.md
CORPUS_DEFINITION_V1.json
PROGRAM_TEMPLATE_CONTRACT_V1.md
AUTHORITY_AND_PROJECT_BOUNDARIES_V1.md
NON_GOALS_V1.md
PROOF_LEDGER_V1.md
STAGE_MAP_V1.md
```

本文と分冊が衝突する場合、より狭く、より具体的で、versionが明示された分冊契約を優先する。契約変更はversion更新と変更理由の記録なしに行ってはならない。

## 0. この仕様の決定

次に実装するものは、Level 4の網羅的拡張でも、万能な数学Semantic IRでもない。

最初の螺旋として、次の問いを端から端まで実証する。

> 一つのPGA Motorによる3次元剛体変換の意味を、早期にCartesian Matrixへ消去せずCompilerまで保持し、同じ意味からMatrix展開方式とDirect PGA方式の二つの正しいGPU Loweringを生成し、独立Verifier、Frozen Leaf Package、Level 4 v1 Frozen Composition、WARP観測、および実GPU計測まで通せるか。

Spiral 1の第一目的は「PGAの方が速い」と証明することではない。

第一目的は、次の新しいCompiler経路を実証することである。

```text
一つの数学的意味
    ↓
表現を保持したCanonical Semantic
    ↓
複数のLowering候補
    ↓
候補生成器とは別系統のVerifier
    ↓
Verified Representation Plan
    ↓
Frozen Leaf Package
    ↓
Level 4 v1 Frozen Composition
    ↓
共通の観測契約による比較
```

PGA方式がMatrix方式より速い、遅い、または同等であっても、上記経路が完全に成立すればArchitecture Gateは成功とする。

---

## 1. 研究上の位置づけ

### 1.1 Level 4とLevel 5の役割

Level 4は、完成済みLeaf Package間の関係を検証済みのFrozen Compositionとして固定する。

Level 5は、Leaf PackageへLoweringされる前の数学的意味を保持し、同じ意味に対する複数の実行方式をCompilerの選択肢として扱う。

Spiral 1ではLevel 4 v1を変更せずにLevel 5の最小概念を試す。

```text
Level 4 v1
  Buffer Flow
  static same-frame DAG
  single writer per Flow
  one DeviceDomain
  headlessまたはsingle presenter
  whole-composition recovery
        ↓
Spiral 1
  PGA意味保持
  二つのLowering
  共通観測
        ↓
不足能力を実験結果から特定
        ↓
必要な場合だけLevel 4を拡張
```

### 1.2 Level 4 v1の扱い

SGE4 Level 4 v1はCanonical Referenceとして固定する。

Spiral 1のDiscovery実装はSGE4へ直接継ぎ足さない。別のSolution、Namespace、成果物identityを用いる。SGE4のProjectをbuild-time dependencyとして参照しない。

推奨名は次とする。

```text
Solution:  SemanticGpuEngine4-5
Namespace: sge4_5
Milestone: Spiral1_RepresentationExperiment1
```

Spiral 1でLevel 4 v1の能力不足が発見されても、実験コードをそのままLevel 4へ昇格させない。必要性、不変条件、拒否条件、Frozen表現、Runtime解釈、Recoveryを別途確定した後にCanonical Reconstructionを判断する。

### 1.3 ABI方針

Spiral 1は、可能な限り既存の証明済みLeaf ABIとRuntime契約を維持する。

PGA固有の数学情報をFrozen Leaf ABIへ無条件に追加しない。既存のShader binary、constant data、Buffer、Compute Workで表現できる限り、Schema 17 / Runtime 17を維持する。

ABI変更は、具体的な不足を文書化し、既存ABIでは正しく表現できないことをNegative Gateで示した場合だけ検討する。

---

## 2. 研究質問と仮説

### 2.1 主研究質問

同一のCanonical PGA Rigid Transform Semanticから、外部観測上同じ剛体変換を実現する二つのGPU Loweringを生成し、その対応関係を独立Verifierが再導出できるか。

### 2.2 実証する仮説

**H1: Representation Retention**

Canonical SemanticはPGA Motorを保持し、Matrixを意味上の正本にしない。

**H2: Multiple Lowering**

同じSemantic identityから、少なくとも次の二候補を生成できる。

```text
A. MatrixExpandedCompute
B. DirectPgaMotorCompute
```

**H3: Independent Authority**

候補生成器の自己申告ではなく、別系統のVerifierが、意味identity、定数、Program template、入出力schema、観測契約を再導出・照合する。

**H4: Frozen Composition Reuse**

二候補は既存Level 4 v1のBuffer Flowだけで同時実行・比較できる。

**H5: Observational Equivalence**

二候補は、CPU binary64 referenceに対して明示されたfloat32誤差契約を満たす。

**H6: Measurable Trade-off**

Matrix展開方式とDirect PGA方式の差を、GPU時間だけでなく、定数bytes、Upload量、Package bytes、演算量、CPU準備時間として記録できる。

### 2.3 仮説に含めないこと

次はSpiral 1の成功条件ではない。

- Direct PGAがMatrixより高速であること
- すべてのGPUで同じ候補が最良であること
- Arbitrary HLSLの数学的正しさを自動証明すること
- PGAがゲーム全体でMatrixより優れること
- Runtimeが毎frame自動的に最適候補を選ぶこと

---

## 3. 用語と権威境界

### 3.1 Canonical Semantic

数学的に実現したい剛体変換を表す正本。Queue、Descriptor、Resource state、Shader binary、Matrix constantを所有しない。

### 3.2 Representation Candidate

Canonical Semanticを特定の実行表現へLoweringした未検証提案。実行もFreezeもできない。

### 3.3 Verified Representation Plan

Independent Representation Verifierが発行したsealを持ち、Canonical Semanticとの対応が再導出された計画。

### 3.4 Frozen Leaf Package

Verified Representation Planだけから生成された、SourceおよびRepresentation Plannerを必要としない実行成果物。

### 3.5 Observation Contract

二つのLoweringが同じ意味を実現したと判定するための、入力範囲、参照計算、誤差metric、tolerance、readback schemaの固定契約。

### 3.6 Completion Gate

Spiral 1は二段階で完成判定する。

```text
Architecture Complete
  表現保持、二候補、独立Verifier、Frozen Composition、WARP資格

Experiment Complete
  Architecture Complete
  + 実GPU計測
  + Decision Report
```

性能優位がなくてもExperiment Completeになり得る。

---

## 4. 対象範囲

### 4.1 対象とする数学的操作

Spiral 1で扱うのは、正規化済みPGA Motorによる有限個の3次元Euclidean pointの剛体変換だけである。

```text
Input:
  one canonical PGA motor
  finite point batch

Operation:
  apply the same rigid transform to every point

Output:
  transformed point batch in the same order
```

### 4.2 対象とするGPU Work

両候補ともCompute Workとする。

Raster、Vertex Shader direct evaluation、Texture、PresentはSpiral 1へ含めない。Compute対Computeに限定し、表現とLowering以外の差を最小化する。

### 4.3 対象とするResource

```text
Input point Buffer
Reference point Buffer
Matrix candidate output Buffer
Direct PGA candidate output Buffer
Comparison record Buffer
Readback Buffer
```

Leaf間共有はBufferのみとし、Level 4 v1の範囲内に留める。

### 4.4 点数

一つのFrozen実験Packageではpoint countを固定する。

Qualification corpusでは、少なくとも次を扱う。

```text
1
3
64
1024
4096
```

可変point count、DispatchIndirect、可変長Bufferは次の螺旋候補であり、Spiral 1へ含めない。

---

## 5. 明示的な非目標

Spiral 1では次を作らない。

- Universal Mathematical Semantic IR
- PGAの一般multivector計算系
- Meet / Join / Line / Plane / General versor
- SDF、CGA、Voxelとの同時比較
- Texture Flow
- Temporal Flow
- Conditional Region
- Runtime Package Variant selection
- Multiple writer
- Streaming / Residency
- Multiple Device / Multiple Adapter
- Editor、Scene Graph、Asset Pipeline
- ゲーム画面の完成
- Arbitrary shader equivalence prover
- 自動最適化探索の一般化

将来必要になりそうという理由だけで、空の型、Capability bit、予約Operationを追加しない。

---

## 6. PGA剛体変換Semantic

### 6.1 Computational representation

Spiral 1では3D PGA Motorの計算表現として、unit dual quaternionと等価な8係数表現を採用する。

```text
Motor = qr + ε qd

qr = real quaternion  (w, x, y, z)
qd = dual quaternion  (w, x, y, z)
```

これはPGA MotorのCanonical computational coordinatesであり、Cartesian Matrixではない。

### 6.2 公開Builder

公開APIは生の8係数を直接受け取らない。

最低限、次のBuilderを提供する。

```text
BuildMotor(UnitQuaternion rotation, Float3 translation)
BuildIdentityMotor()
```

任意係数の直接構築は、Corruption test専用の内部経路に限定する。

### 6.3 Canonical construction

Unit quaternionを`qr`、translationをpure quaternion `t = (0, tx, ty, tz)`とすると、Canonical motorを次で構築する。

```text
qd = 0.5 * t * qr
Motor = qr + ε qd
```

`qr`はbinary64で正規化し、同じ回転を表す`qr`と`-qr`を一意化する。

符号のCanonical ruleは、`(w, x, y, z)`の順で最初に絶対値が`signEpsilon`を超える係数を正にすることとする。`qr`を反転する場合は`qd`も同時に反転する。

### 6.4 Canonical Semantic record

概念型は次とする。

```text
PgaMotorSemantic
  qr[4] : IEEE-754 binary64
  qd[4] : IEEE-754 binary64
  conventionVersion

PointBatchSemantic
  elementKind = Float4Point
  pointCount
  inputCorpusIdentity

ApplyPgaMotorSemantic
  motorIdentity
  pointBatchSchemaIdentity
  outputSchemaIdentity
  observationContractIdentity
```

Semantic recordにはMatrix、Shader、Queue、Dispatch group count、Buffer stateを置かない。

### 6.5 Point schema

GPU上の点は固定16-byte recordとする。

```text
Float4Point
  x : binary32
  y : binary32
  z : binary32
  w : binary32 = 1.0
```

入力では、全成分がfiniteであり、`w`のbit patternが`1.0f`であることを要求する。

Qualification profileでは、`abs(x), abs(y), abs(z) <= 10000`とする。

### 6.6 Canonical serialization

Semantic bytesは次を満たす。

- 固定幅整数とIEEE-754 bit patternを用いる
- `-0.0`を`+0.0`へ正規化する
- NaNとInfinityを拒否する
- recordと係数順を固定する
- timestamp、pointer、random UUIDを含めない
- 同じ意味入力からDebug / Release / fresh processでbyte-identicalになる

### 6.7 Semantic identity

`SemanticIdentity`は少なくとも次から計算する。

```text
motor canonical bytes
motor convention version
point schema
point count
input corpus identity
output schema
observation contract identity
```

Lowering kind、Target profile、Shader digestはSemantic identityへ含めない。それらはCandidate identityに属する。

---

## 7. CPU reference

### 7.1 Reference authority

CPU binary64 referenceは、GPU二候補とは独立した観測基準とする。

Reference pathはMatrix candidateの実装を呼び出してはならず、Direct PGA kernelのC++移植をそのまま共有してはならない。

### 7.2 Reference transform

Canonical motorから、binary64でrotationとtranslationを評価する。

概念的には次である。

```text
rotated = qr * pointQuaternion * conjugate(qr)
translationQuaternion = 2 * qd * conjugate(qr)
result = rotated.xyz + translationQuaternion.xyz
```

出力はbinary64で計算後、qualification用reference bufferへbinary32 round-to-nearest-ties-to-evenで変換する。

### 7.3 Reference artifact

Reference point bytesはinput corpus、motor、reference algorithm versionから決定的に生成し、Corpus Source LeafのPackage-owned initial dataとして保存する。

実行時にCPU referenceを再計算して判定を変えてはならない。診断用の再計算は許可するが、Frozen experiment identityには影響させない。

---

## 8. Lowering A：MatrixExpandedCompute

### 8.1 意味

CompilerがCanonical PGA MotorからCartesian affine transformを導出し、Compute kernelへMatrix定数として渡す。

### 8.2 固定表現

最初の版ではrow/columnの曖昧さを避けるため、3x4 affine matrixを明示的な12個のbinary32係数として保存する。

```text
m00 m01 m02 tx
m10 m11 m12 ty
m20 m21 m22 tz
```

Matrix storage order、vector convention、multiplication directionは`MatrixExpandedComputeV1`で固定する。

### 8.3 導出

Target CompilerはCanonical binary64 motorからbinary64 matrixを導出し、各係数をbinary32へ一度だけ丸める。

Matrix CandidateにPGA Motor係数を残す必要はないが、Candidate manifestは元Semantic identityを参照する。

### 8.4 Program template

任意HLSLを受け入れない。

`CanonicalMatrixPointTransformCS_V1`という固定Program templateを使用する。Target profileごとに許可されたShader binary digest、binding layout digest、thread-group sizeを固定する。

### 8.5 出力

入力と同じ順序で`Float4Point`を出力し、`w = 1.0f`を保証する。

---

## 9. Lowering B：DirectPgaMotorCompute

### 9.1 意味

Canonical PGA Motorの8係数をMatrixへ展開せず、Compute ShaderがMotor表現のまま各pointへ剛体変換を適用する。

### 9.2 固定表現

Canonical binary64 motorを、係数順を保って8個のbinary32へ丸める。

```text
qr.w qr.x qr.y qr.z
qd.w qd.x qd.y qd.z
```

### 9.3 Program template

任意HLSLを受け入れない。

`CanonicalDirectPgaMotorPointTransformCS_V1`を固定Program templateとし、Target profileごとにShader binary digest、binding layout digest、thread-group sizeを固定する。

### 9.4 禁止事項

Direct PGA Candidate生成前にMatrixへ展開し、そのMatrixを隠れた定数としてShaderへ渡してはならない。

Verifierが検査できるよう、constant schemaは8個のMotor係数だけとする。

### 9.5 出力

入力と同じ順序で`Float4Point`を出力し、`w = 1.0f`を保証する。

---

## 10. Candidate identityとAuthority chain

### 10.1 Authority chain

正式な権威経路を次に限定する。

```text
PgaRigidTransformExperimentSpec
    ↓
Canonical Pga Semantic
    ↓
Representation Candidate Generator
    ↓
Raw Representation Candidate
    ↓
Independent Representation Verifier
    ↓
Verified Representation Plan
    ↓
Leaf Target Compiler
    ↓
Frozen Leaf Package
    ↓
Level 4 Composition Contract
    ↓
Composition Planner
    ↓
Independent Composition Verifier
    ↓
Frozen Composition
    ↓
Runtime / D3D12 Executor
```

### 10.2 Raw Candidateの制限

Raw Representation Candidateは次を行えない。

- Frozen Packageの生成
- Runtime実行
- Compositionへの登録
- Verified identityの自己生成
- Verifier sealの再計算による偽装

### 10.3 Candidate identity

Candidate identityは少なくとも次を含む。

```text
Semantic identity
Lowering kind
Lowering version
Target profile identity
Program template identity
Program binary digest
Binding layout digest
Input schema identity
Output schema identity
Observation contract identity
Constant payload digest
```

MatrixとDirect PGAは異なるCandidate identityとPackage execution digestを持つ。

---

## 11. Independent Representation Verifier

### 11.1 分離原則

VerifierはCandidate Generatorの関数を呼び出して同じ結果を再生するだけであってはならない。

Canonical Semanticを入力とし、独立した小さな再導出実装を持つ。

### 11.2 Matrix Candidateの検査

Verifierは次を行う。

1. Semantic identityを再計算する
2. Canonical motorからbinary64 matrixを独立再導出する
3. 規定roundingでbinary32 matrix bytesを生成する
4. Candidate constant payloadとbyte比較する
5. Matrix storage conventionとversionを確認する
6. Canonical Matrix Program template digestを確認する
7. input/output Buffer schema、stride、point countを確認する
8. Observation Contract identityを確認する

### 11.3 Direct PGA Candidateの検査

Verifierは次を行う。

1. Semantic identityを再計算する
2. Canonical binary64 motorを規定roundingで8個のbinary32へ変換する
3. Candidate constant payloadとbyte比較する
4. coefficient orderとconvention versionを確認する
5. Canonical Direct PGA Program template digestを確認する
6. input/output Buffer schema、stride、point countを確認する
7. Observation Contract identityを確認する

### 11.4 Program意味の境界

Spiral 1はarbitrary shader theorem proverを作らない。

Program意味の信頼は、少数のCanonical Program templateをallowlist化し、そのbinary digestとinterface digestをVerifierが確認することで閉じる。

TemplateのHLSL変更、Compiler target変更、DXIL変更はProgram template versionまたはTarget profile identityを変更しなければならない。

### 11.5 Seal

Verified Representation PlanはVerifierだけが生成できるopaque sealを持つ。

sealは少なくとも次に結びつく。

```text
Semantic identity
Candidate identity
Verifier schema version
Target profile identity
Observation contract identity
```

---

## 12. Frozen Leaf構成

### 12.1 Leaf一覧

Spiral 1のCanonical Compositionは4 Leafとする。

```text
L0 CorpusAndReferenceSource
L1 MatrixExpandedTransform
L2 DirectPgaMotorTransform
L3 PointComparison
```

### 12.2 L0 CorpusAndReferenceSource

Package-owned initial dataから、次の二つのComposition Bufferを生成する。

```text
InputPoints
ReferencePoints
```

L0は入力点とCPU referenceを同じscenario identityに結びつける。

### 12.3 L1 MatrixExpandedTransform

`InputPoints`を読み、`MatrixOutputPoints`へ書く。

### 12.4 L2 DirectPgaMotorTransform

`InputPoints`を読み、`DirectPgaOutputPoints`へ書く。

### 12.5 L3 PointComparison

次を読む。

```text
ReferencePoints
MatrixOutputPoints
DirectPgaOutputPoints
```

各pointについて決定的なComparison Recordを書き出す。

---

## 13. Level 4 v1 Composition Contract

### 13.1 Flow

```text
L0 --InputPoints----------> L1
L0 --InputPoints----------> L2
L0 --ReferencePoints------> L3
L1 --MatrixOutputPoints---> L3
L2 --DirectPgaOutputPoints> L3
L3 --ComparisonRecords----> Composition Output / Readback
```

### 13.2 Level 4 v1制約との一致

- Shared ResourceはBufferのみ
- 全Flowはsingle writer
- `InputPoints`はfan-out read
- Graphはfinite static same-frame DAG
- presenterは0
- DeviceDomainは1
- Adapterは1
- Recovery unitはComposition全体

### 13.3 Runtime権限

Runtimeは次を再判断しない。

- どちらのLoweringが正しいか
- Leafの実行順序
- Buffer allocation identity
- producer / consumer関係
- state handoff
- signal / wait
- 比較対象の対応付け
- Observation Contract

これらはVerified Composition PlanとFrozen Compositionで固定する。

### 13.4 Level 4拡張禁止

Spiral 1 Architecture Gateが閉じるまで、Texture Flow、Variant Set、Conditional Regionなどを追加しない。

Level 4 v1だけでは不可能だった場合、失敗を隠す一時拡張を行わず、不足能力をDecision Reportへ記録する。

---

## 14. Observation Contract

### 14.1 Comparison Record

GPU Compare Leafは、浮動小数点reductionを行わず、pointごとに固定recordを書き出す。

概念型は次とする。

```text
PointComparisonRecordV1
  pointIndex
  finiteFlags
  matrixAbsErrorX/Y/Z
  pgaAbsErrorX/Y/Z
  pairAbsErrorX/Y/Z
  matrixUlpDistanceX/Y/Z
  pgaUlpDistanceX/Y/Z
  pairUlpDistanceX/Y/Z
  mismatchFlags
```

RMSやsumをGPU atomicで集計しない。readback後、CPU Observerがpoint index昇順で決定的に集計する。

### 14.2 Pass metric

Qualification profileの既定値を次とする。

```text
absoluteTolerance = 1.0e-4
relativeTolerance = 1.0e-5
pairAbsoluteTolerance = 1.0e-4
```

各成分について次を満たすことを要求する。

```text
abs(actual - reference)
  <= absoluteTolerance
   + relativeTolerance * max(abs(reference), 1.0)
```

MatrixとDirect PGAの相互差にも同じ形式を適用する。

ToleranceはObservation Contract bytesへ含め、実行後に自由変更できない。

### 14.3 必須観測

各scenarioで少なくとも次を記録する。

```text
point count
non-finite count
mismatch count
first mismatch index
max absolute error
max relative error
max ULP distance
RMS Euclidean error
readback digest
```

### 14.4 決定性

同じPackage、WARP環境、入力bytesではComparison Record readback bytesがfresh process間で一致することを要求する。

異なる実GPUやDriver間でのbyte一致は要求しない。異なる環境ではObservation Contract passだけを要求する。

---

## 15. Canonical qualification corpus

Corpusは固定seed、固定生成規則、固定point順序を持つ。時刻や実行環境に依存する乱数を使わない。

最低限、次のscenarioを含める。

| ID | Motor | Point set | 主目的 |
|---|---|---|---|
| S00 | Identity | 1 point | 最小経路 |
| S01 | Identity | 1024 grid points | 大量identity |
| S02 | Translation only | 64 signed points | translation |
| S03 | X axis +90° | axis and off-axis points | rotation X |
| S04 | Y axis +90° | axis and off-axis points | rotation Y |
| S05 | Z axis +90° | axis and off-axis points | rotation Z |
| S06 | arbitrary axis 180° | 64 points | half-turn |
| S07 | rotation + translation | 1024 points | combined transform |
| S08 | very small angle | 1024 points | near-identity precision |
| S09 | angle near π | 1024 points | sign / precision boundary |
| S10 | three composed rigid transforms | 1024 points | composed motor |
| S11 | negative coordinate cloud | 4096 points | signed range |
| S12 | large coordinate cloud | 4096 points | magnitude range |
| S13 | very small coordinates | 4096 points | sub-unit precision |
| S14 | fixed-seed random transform | 4096 points | broad deterministic sample |
| S15 | fixed-seed random corpus set | 16 motors × 256 points | corpus breadth |

Exact motor、point generation rule、seedは`Spiral1CorpusV1`のversioned specificationとして固定する。

---

## 16. Negative Gate

### 16.1 Semantic Negative Gate

次を拒否する。

- NaN / Infinityを含むrotation、translation、point
- zero-length quaternion
- unit toleranceを満たさないrotation
- non-canonical quaternion signのserialized Semantic
- `w != 1.0f`のpoint
- point count 0
- qualification上限を超えるpoint count
- 不明なMotor convention version
- Observation Contract欠落

### 16.2 Candidate Authority Negative Gate

次を拒否する。

- Matrix係数1要素の改変
- Matrix storage orderの偽装
- Motor係数1要素の改変
- qr / qd係数の入替え
- 別Semantic identityの混入
- 異なるpoint count
- input/output stride改変
- Program template digest改変
- Binding layout digest改変
- Observation Contract identity差替え
- Raw Candidateからの直接Freeze
- sealのcopy / replay
- Target profile差替え

### 16.3 Composition Negative Gate

次を拒否する。

- `InputPoints`のwriter追加
- Matrix outputとDirect PGA outputの接続交換
- ReferencePoints未接続
- Compare Leafのconsumer欠落
- cycle追加
- presenter追加
- 別DeviceDomainのLeaf混入
- allocation reference改変
- state handoff改変
- signal / wait改変
- stale device epoch resource

### 16.4 Package / Runtime Negative Gate

既存のPackage corruption、digest、schema、unknown operation、external mismatch、device epoch検査を維持する。

---

## 17. Recovery資格

### 17.1 Recovery unit

Spiral 1 Composition全体を一つのRecovery unitとする。

Device loss時は次を破棄する。

```text
all Leaf instances
all shared Buffers
all completion tokens
all endpoint state
all readback state
```

Device epochを進め、同じFrozen Compositionから再物質化する。

### 17.2 必須Recovery scenario

少なくともS07を用いて次を検証する。

```text
Load Frozen Composition
Run and read back
Controlled recovery
Rematerialize
Run and compare
Actual RemoveDevice path
Exclude removed LUID
Rematerialize or AwaitingAdapter
Run when eligible adapter is available
```

Recovery前後で、同一WARP環境のComparison Record bytesとreadback digestが一致することを要求する。

古いBuffer、token、submissionは新epochで拒否する。

---

## 18. Determinism Gate

次の成果物は同一入力・同一Target profileでbyte-identicalでなければならない。

```text
Canonical Semantic bytes
Semantic digest
Raw Candidate manifest bytes
Verified Representation Plan bytes
Frozen Leaf Package bytes
Composition Contract bytes
Verified Composition Plan bytes
Frozen Composition bytes
Canonical corpus manifest
Observation Contract bytes
```

比較条件は次とする。

```text
fresh Debug process A
fresh Debug process B
fresh Release process
```

Provenanceやdebug-only情報をexecution identityへ混入させない。

---

## 19. テスト階層と運用

### 19.1 Dev Tier

高速な意味・候補・Verifier試験。

```text
run_spiral1_dev.bat
```

対象:

- Motor Builder
- Semantic canonicalization
- CPU reference
- Matrix再導出
- Direct coefficient rounding
- Candidate identity
- Negative mutations

### 19.2 Gate Tier

全project buildとAuthority gate。

```text
run_spiral1_gate.bat
```

対象:

- Debug x64 build
- Release x64 build
- raw candidate execution拒否
- verifier seal authority
- Leaf package validation
- Composition authority tests

### 19.3 Regression Tier

WARP実行とreadback corpus。

```text
run_spiral1_regression.bat
```

対象:

- S00-S15
- Matrix vs reference
- Direct PGA vs reference
- Matrix vs Direct PGA
- controlled recovery
- actual removal qualification

### 19.4 Freeze Tier

fresh-process determinismと最終Manifest。

```text
run_spiral1_freeze.bat
```

対象:

- Debug fresh process×2
- Release fresh process
- byte comparison
- package corruption suite
- recovery suite
- completion banner

### 19.5 テスト台帳

各テストは次を記録する。

```text
Invariant ID
Owner project
Positive scenario
Negative mutation
Expected rejection stage
Runtime observation
Recovery observation
Tier
```

実装は作り直してよいが、証明済みInvariantを理由なく削除してはならない。

---

## 20. 実GPU Performance Experiment

### 20.1 位置づけ

WARPは正当性と再現性の証明に用い、性能主張には用いない。

Architecture Complete後に実GPUで計測する。

### 20.2 固定Benchmark profile

Benchmarkは同じAdapter fingerprint、driver、clock policy、point corpus、dispatch countを記録する。

推奨手順:

```text
warm-up: 200 iterations
measurement: 2000 iterations
order: ABBA blocks
repeat: 5 runs
report: median and p95
```

環境上実行時間が過大な場合は回数を変更できるが、変更値をProfile identityへ含める。

### 20.3 測定項目

```text
Compiler candidate generation time
Verifier time
Freeze time
Package bytes
constant bytes
CPU-to-GPU upload bytes
materialization time
command recording CPU time
GPU dispatch time
end-to-end submission time
allocated bytes
readback bytes
numeric error metrics
```

### 20.4 性能判定

高速・低メモリであることをGateにしない。

結果は次のいずれでもよい。

```text
Matrix優位
Direct PGA優位
point countにより交差
Adapterにより交差
差が測定noise以下
Direct PGAは遅いが転送量が小さい
```

重要なのは、同じ意味・同じ観測・固定成果物で公平に比較できることである。

---

## 21. 診断要件

### 21.1 Compiler diagnostics

少なくとも次を説明する。

- なぜSemanticを拒否したか
- どのMotor係数、point、schemaが不正か
- どのLowering候補を生成したか
- MatrixまたはMotor定数をどの規則で導出したか
- CandidateがなぜVerifierに拒否されたか

### 21.2 Runtime diagnostics

Source表現を直接知らず、次を基準にする。

```text
Frozen Composition identity
Leaf identity
Flow identity
Package section / record
Operation index
device epoch
scenario identity
```

### 21.3 Comparison diagnostics

mismatch時は少なくとも次を出力する。

```text
scenario ID
point index
input point
reference point
matrix output
Direct PGA output
absolute error
relative error
ULP distance
Motor identity
Candidate identities
Package digests
```

---

## 22. 実装Stage

### Stage 03：Spiral 1 Research Contract Freeze

- 本仕様v0.3と分冊契約を固定
- Corpus V1の生成規則・seed・scenario identityを固定
- Observation Contract V1のbinary layout、tolerance、集計順を固定
- Canonical Program Template V1のinterfaceとcompile profileを固定
- Authority chain、ProjectReference境界、Negative Gateを固定
- F0のC++／Visual Studio project群を変更していないことをdigestで証明

完成条件: C++機能追加なしで、入力、意味、候補、Verifier、Frozen境界、観測、拒否、次Stageを一意に説明でき、`run_sge4_5_stage03_contract.bat`が合格する。

### Stage 04：PGA Math and Canonical Semantic

- UnitQuaternion Builder
- Canonical PGA Motor Builder
- Point schema
- Semantic serialization / digest
- Semantic Negative Gate

完成条件: Matrixを正本へ混入させず、同じ意味入力からfresh processで同一Semantic bytesを生成する。

### Stage 05：Corpus, CPU Reference and Observation Foundation

- S00-S15 canonical corpus generator
- CPU binary64 reference
- Reference artifact bytes
- PointComparisonRecord V1
- deterministic CPU observer

完成条件: Corpus、reference、Observation Contractの全bytesが決定的で、GPU候補なしに期待観測を説明できる。

### Stage 06：Representation Candidate Generation

- MatrixExpandedCompute raw candidate
- DirectPgaMotorCompute raw candidate
- Candidate manifest / identity

完成条件: 同じSemantic identityから、同一I/O契約を持つ異なる二候補を生成する。

### Stage 07：Independent Representation Authority

- Matrix independent rederivation
- Motor coefficient independent verification
- Program template allowlist
- opaque verifier seal
- candidate mutation tests

完成条件: Raw CandidateはFreezeも実行もできず、主要な改変をVerifierが拒否する。

### Stage 08：Matrix Leaf Vertical Slice

- Matrix candidateからVerified Representation Plan
- Existing ExecutionPlan authorityへの接続
- Frozen Matrix Leaf Package
- leaf-only WARP readback

完成条件: Matrix経路がSource／Plannerなしで再物質化・実行・Recoveryできる。

### Stage 09：Direct PGA Leaf Vertical Slice

- Direct PGA canonical shader template
- 8-coefficient constant schema
- Frozen Direct PGA Leaf Package
- leaf-only WARP readback

完成条件: Direct PGA経路がMatrixを隠れて利用せず、CPU reference契約を満たす。

### Stage 10：Four-Leaf Frozen Comparison Composition

- L0 CorpusAndReferenceSource
- L1 MatrixExpandedTransform
- L2 DirectPgaMotorTransform
- L3 PointComparison
- Level 4 v1 Buffer Flow接続

完成条件: Level 4 v1 ABIを拡張せず、4 LeafのVerified Frozen Compositionを生成する。

### Stage 11：WARP Observation Corpus

- S00-S15 execution
- Matrix vs reference
- Direct PGA vs reference
- Matrix vs Direct PGA
- deterministic readback aggregation

完成条件: 全scenarioがObservation Contract V1を満たす。

### Stage 12：Recovery, Authority and Determinism Freeze

- controlled recovery
- actual RemoveDevice
- removed-LUID exclusion / AwaitingAdapter
- stale epoch rejection
- fresh Debug A/B and Release comparison

完成条件: Completion Invariants S1-I01からS1-I16が合格し、Architecture Completeを宣言できる。

### Stage 13：Real GPU Measurement

- Adapter fingerprint profile
- ABBA benchmark
- numerical and performance report

完成条件: 性能優劣に関係なく、同一条件の再現可能な比較reportを生成する。

### Stage 14：Decision Report and Spiral 1 Final Freeze

- Level 4 v1で十分だったかを判定
- Representation保持の実利を判定
- 次の螺旋またはLevel 4拡張を一つだけ選択
- Proof Ledgerを最終化

完成条件: 次の実装を予測ではなく観測証拠から選べる。

## 23. 推奨Project構成

```text
00_Base
01_MathVocabulary
02_PgaSemantic
03_RepresentationContracts
04_RepresentationCandidatePlanner
05_RepresentationVerifier
06_LeafTargetCompiler
07_FrozenLeafSchema
08_CompositionContracts
09_CompositionPlanner
10_CompositionVerifier
11_FrozenComposition
12_PackageRuntime
13_D3D12Executor
14_Spiral1Corpus
15_Spiral1Observer
30_SemanticTests
31_RepresentationAuthorityTests
32_LeafPackageTests
33_CompositionAuthorityTests
34_WarpObservationTests
35_RecoveryTests
36_DeterminismTests
37_PerformanceExperiment
40_Spiral1Launcher
```

既存SGE4の正確なProject分割を機械的に再現する必要はない。ただし、PlannerとVerifier、SourceとFrozen、RuntimeとCompiler、BackendとSemanticの禁止依存をProjectReferenceで強制する。

---

## 24. Completion Invariants

| ID | 不変条件 |
|---|---|
| S1-I01 | Canonical SemanticにCartesian Matrixが存在しない |
| S1-I02 | 同じSemantic identityから二つのLowering候補を生成できる |
| S1-I03 | Raw CandidateはFreezeも実行もできない |
| S1-I04 | VerifierがMatrix定数を独立再導出する |
| S1-I05 | VerifierがDirect PGA係数を独立再導出する |
| S1-I06 | Arbitrary Shaderではなくversioned Canonical Program templateだけを受理する |
| S1-I07 | 二候補の入出力schemaとObservation Contractが同一である |
| S1-I08 | Level 4 v1 Buffer Flowだけで比較Compositionを構成できる |
| S1-I09 | 各internal Flowはexactly one writerを持つ |
| S1-I10 | RuntimeはLowering選択・schedule・state・syncを再判断しない |
| S1-I11 | S00-S15がCPU reference誤差契約を満たす |
| S1-I12 | MatrixとDirect PGAの相互差が誤差契約を満たす |
| S1-I13 | Comparison集計はpoint順に決定的である |
| S1-I14 | Frozen成果物がDebug / Release / fresh processでbyte-identicalである |
| S1-I15 | Controlled recoveryとactual removal後に同じ観測を再現する |
| S1-I16 | stale epoch objectとtokenを拒否する |
| S1-I17 | 性能結果に関係なく公平な比較reportを生成できる |
| S1-I18 | 次のLevel 4拡張はDecision Reportの証拠なしに追加されない |

---

## 25. 完成判定

### 25.1 Architecture Complete

次をすべて満たしたときだけ宣言する。

- S1-I01からS1-I16がすべて合格
- Raw Candidate authority gateが合格
- Representation mutation testsが合格
- Level 4 Composition authority testsが合格
- WARP S00-S15が合格
- controlled / actual recoveryが合格
- fresh Debug / Release determinismが合格
- Schema / Runtimeの変更がある場合、その必要性とversioningが別仕様で承認済み

推奨banner:

```text
============================================================
SGE4-5 SPIRAL 1 REPRESENTATION AUTHORITY PASSED
Canonical meaning: PGA rigid transform retained through candidate generation
Lowerings: MatrixExpandedCompute / DirectPgaMotorCompute
Authority: raw candidates are non-executable; independent verifier seals plans
Composition: Level 4 v1 Buffer-only static DAG
Observation: CPU reference, pairwise comparison and deterministic readback qualified
Recovery: whole-composition rematerialization qualified
============================================================
```

### 25.2 Experiment Complete

Architecture Completeに加え、次を満たす。

- 少なくとも一つの実GPU AdapterでBenchmark profileを取得
- Matrix / Direct PGAの性能とResource差を同一条件で記録
- Error metricsとperformance metricsを同じreportへ保存
- Decision Reportを作成
- 次の螺旋またはLevel 4拡張候補を一つに絞る

推奨banner:

```text
============================================================
SGE4-5 SPIRAL 1 EXPERIMENT COMPLETE
Representation-preserving multi-lowering experiment completed.
No performance superiority is implied by completion.
Next step is selected from observed evidence, not anticipated generality.
============================================================
```

---

## 26. Decision Reportで必ず答える問い

1. Level 4 v1だけで実験を閉じられたか。
2. 閉じられなかった場合、正確にどの契約が不足したか。
3. その不足はLevel 4の一般能力として追加すべきか、実験固有Leaf内部へ閉じるべきか。
4. SemanticをPGAとして保持したことで、Matrixへ即時変換する場合には存在しない候補が生成されたか。
5. Direct PGAは、演算時間、定数bytes、Upload量、Package容量のどこで有利・不利だったか。
6. point countまたはAdapterによって優位候補が変わったか。
7. Independent VerifierはPlannerのどの改変を検出できたか。
8. Arbitrary Shader proofを導入せず、Canonical Program template方式で十分だったか。
9. 次の実験は、動的Motor、可変point count、Vertex Shader direct evaluation、階層Transformのどれであるべきか。
10. 次にLevel 4へ追加する能力は本当に必要か。追加しない選択が妥当か。

---

## 27. Spiral 1後の候補

次は事前確定しない。Decision Reportから一つだけ選ぶ。

```text
Spiral 2A: 毎frame変化するPGA Motor / Matrix palette
Spiral 2B: 可変point countとGPU-generated work
Spiral 2C: Vertex Shader direct PGA evaluation
Spiral 2D: 階層剛体TransformとAnimation preparation
Spiral 2E: Verified Package Role / Variant Set
Spiral 2F: 限定Texture2D Observation Flow
```

選ばれなかった能力は空の抽象として実装しない。

---

## 28. 最終原則

Spiral 1の価値は、PGAを採用することそのものではない。

価値は、数学的意味を早期に消去せず、複数の実行方式を生成し、候補生成器とは別のVerifierで対応関係を確認し、その結果を既存のFrozen実行境界へ接続できるかを実証することにある。

```text
意味を保持する
    しかしBackendへ数学を漏らさない

候補を増やす
    しかしRuntimeへ再判断を戻さない

実験する
    しかし未実証の一般化を先に作らない

性能を測る
    しかし速さを完成条件にしない

不足を発見する
    その不足だけが次のLevel 4拡張を正当化する
```

これを、Level 4とLevel 5を相互に導く最初の正式な一周とする。
