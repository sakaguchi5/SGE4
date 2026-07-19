# SGE4-5 Spiral 2 完成ロードマップ

## 1. 正式名称

Spiral 2: Fixed Dynamic Motor Palette and Hierarchical Representation Composition

日本語名：

固定長動的Motor Paletteと階層剛体変換の表現保持・Composition実験

基準実装：

```text
Repository:
  sakaguchi5/SGE4

Baseline:
  afaed65d9400f3f80a7f5a9094edd9029082f95d

Foundation:
  Level 4 v1 Canonical Final Integration Freeze
  Spiral 1 Architecture Complete
  Spiral 1 Real GPU Measurement Complete
```

## 2. Spiral 2の研究質問

主研究質問は次とする。

> 固定されたSkeleton topologyとBone数を持つ階層剛体変換について、毎frame供給されるLocal PGA Motor Paletteを数学的正本として維持し、同じ意味から異なるLeaf群を生成し、そのLeaf群をLevel 4のFrozen Compositionとして固定・実行・回復・比較できるか。

Spiral 1は、

```text
一つの固定Motor
→ Matrix Lowering
→ Direct PGA Lowering
```

という、主に一つのLeaf内部の表現差を証明した。

Spiral 2は、

```text
動的Local Motor Palette
+
固定Hierarchy
        ↓
異なるLeaf群
異なる中間Buffer
異なるLowering位置
異なるComposition形状
```

を扱う。

つまり、Spiral 2では初めて、

> 同じ意味から異なるPackage／Leaf群そのものを生成する

というLevel 5の本格形を、Level 4へ通す。

Level 4は完成済みLeaf間の関係を固定し、Level 5はLeafになる前の意味から複数の実行方式を生成する、というSpiral 1の役割分担を維持する。

# 3. 完成対象の厳密な範囲

## 3.1 固定される構造

次はFrozen SemanticまたはFrozen成果物のidentityに含まれる。

```text
Bone count
Parent index table
Root identity
Canonical bone order
Depth layer table
Motor convention version
Dynamic palette schema
Probe-point schema
Output schema
Observation Contract
Candidate graph kind
Program template versions
Target profile
```

Bone数または親子関係が変わった場合は再Compileする。

## 3.2 毎frame変化する値

次はFrozen identityへ含めない。

```text
Local Motor Palette
Frame number
動的Reference output
Invocation identity
```

同じFrozen Compositionへ、毎frame異なるPaletteを渡せなければならない。

設計憲法上も、Camera matrixやCPU更新Bufferなどの動的値はFrameInvocationとしてPackageと分離し、動的値の変更だけでPackageを再Compileしてはならない。

## 3.3 対象とするHierarchy

最初の版では次へ限定する。

```text
一つのroot
Bone count固定
Parent index固定
parentIndex < childIndex
有限非循環tree
Scaleなし
Rigid transformのみ
同一frame内で完結
```

複数root forestは対象外とする。

## 3.4 明示的な非目標

Spiral 2では次を作らない。

```text
Skinning
Vertex influence
Animation clip evaluation
Animation blending
IK
Constraint solver
Temporal history
Previous-frame pose
Motion vector
可変Bone count
可変Skeleton topology
DispatchIndirect
可変長Buffer
Runtime Variant selection
Conditional Region
Texture Flow
Streaming / Residency
Multiple Device / Adapter
Partial Recovery
一般PGA multivector
任意Shader証明
```

将来用の空型、Capability bit、予約Operationも追加しない。

# 4. Canonical Semantic

## 4.1 静的Hierarchy Semantic

概念型を次とする。

```text
RigidHierarchySemanticV1
  boneCount
  rootBone
  parentIndices[boneCount]
  canonicalBoneOrder[boneCount]
  depthOffsets[]
  depthBoneIndices[]
  localMotorSchemaIdentity
  globalTransformMeaningIdentity
  probeSchemaIdentity
  observationContractIdentity
```

Canonical Semanticは、次を持たない。

```text
Matrix
Shader binary
Queue
Descriptor
Resource state
Allocation
Dynamic local Motor values
```

## 4.2 動的Local Motor Palette

毎frameの入力を次とする。

```text
DynamicLocalMotorPaletteV1
  boneCount
  motors[boneCount]

Motor record:
  qr.w qr.x qr.y qr.z
  qd.w qd.x qd.y qd.z
```

各係数はbinary32とする。

全候補は完全に同じ入力bytesを読む。

Matrix候補だけCPU側でMatrix Paletteを作って渡すことは禁止する。そうすると、入力表現とUpload量が候補ごとに変わり、公平な比較にならないためである。

## 4.3 Dynamic Invocation Builder

実行時のPGA意味検証をRuntimeやBackendへ漏らさない。

正式経路は次とする。

```text
rotation + translation
        ↓
Dynamic Motor Palette Builder
        ↓
canonical binary32 Motor bytes
        ↓
FrameInvocation
        ↓
Frozen Composition Runtime
```

Builderは少なくとも次を拒否する。

```text
NaN / Infinity
zero-length quaternion
unit tolerance違反
非canonical符号
Bone count不一致
byte size不一致
未知Motor convention
```

Runtime Coreが知るのはDynamic Slotのidentity、必要bytes、alignmentだけとする。

Frozen Package ABIでもFrameInvocationはDynamic Slotに対して完全一致するサイズを要求し、余分・欠落・重複slotを拒否する設計である。

# 5. 三つの候補Leaf群

## Candidate A：Matrix Hierarchy

```text
Local PGA Motor Palette
        ↓
A0 LocalMotorToMatrix
        ↓
Local Matrix Palette
        ↓
A1 MatrixHierarchy
        ↓
Global Matrix Palette
        ↓
A2 MatrixProbeApply
        ↓
Matrix Observation Points
```

特徴：

```text
Matrix化位置:
  hierarchyの前

中間表現:
  3x4 affine matrix

主なCost:
  local Motor→Matrix変換
  Matrix hierarchy composition
  48 bytes × Bone数のpalette
```

## Candidate B：Direct PGA Hierarchy

```text
Local PGA Motor Palette
        ↓
B0 MotorHierarchy
        ↓
Global PGA Motor Palette
        ↓
B1 DirectPgaProbeApply
        ↓
Direct PGA Observation Points
```

特徴：

```text
Matrix化:
  しない

中間表現:
  8係数Motor

主なCost:
  Motor composition
  Direct PGA point apply
  32 bytes × Bone数のpalette
```

## Candidate C：Hybrid Hierarchy

```text
Local PGA Motor Palette
        ↓
C0 MotorHierarchy
        ↓
Global PGA Motor Palette
        ↓
C1 GlobalMotorToMatrix
        ↓
Global Matrix Palette
        ↓
C2 MatrixProbeApply
        ↓
Hybrid Observation Points
```

特徴：

```text
Matrix化位置:
  hierarchyの後

中間表現:
  HierarchyまではMotor
  最終apply前にMatrix

主な研究対象:
  PGA合成の利点
  Matrix大量適用の利点
  両者の境界位置
```

この三候補によって、

```text
早期Matrix化
Matrix化しない
遅延Matrix化
```

を公平に比較できる。

# 6. 共通Observation Contract

候補A、B、Cは内部表現が異なるため、Matrix bytesやMotor bytesそのものを共通出力にはしない。

各BoneのGlobal rigid transformを、固定されたProbe pointsへ作用させた結果を共通観測とする。

## 6.1 BoneごとのProbe points

最低限、次を用いる。

```text
P0 = (0, 0, 0, 1)
P1 = (1, 0, 0, 1)
P2 = (0, 1, 0, 1)
P3 = (0, 0, 1, 1)
P4 = (0.37, -0.61, 1.13, 1)
```

原点と三つの基底点によって、剛体変換のtranslationとrotationを観測できる。

非対称点P4は、軸対称ケースだけでは見えにくい誤りを検出する。

## 6.2 CPU Reference

CPU Referenceは、

```text
dynamic binary32 local Motor bytes
        ↓ binary64へ昇格
独立Motor composition
        ↓
global binary64 transform
        ↓
fixed Probe pointsへ適用
        ↓
binary32 reference output
```

で生成する。

候補AのMatrix実装、候補BのPGA kernel、候補Cの変換関数を共有してはならない。

## 6.3 比較項目

各Bone・各Probe・各成分について記録する。

```text
absolute error
relative error
ULP distance
non-finite
pairwise A-B
pairwise A-C
pairwise B-C
```

追加で、変換後の基底から次を導出する。

```text
axis length error
axis orthogonality error
orientation / determinant sign
translation error
```

# 7. Canonical Composition

全候補を一つの比較Compositionへ入れる場合の標準形を次とする。

```text
L0 DynamicInvocationSource
  ├─ LocalMotorPalette──────────────> A0
  ├─ LocalMotorPalette──────────────> B0
  ├─ LocalMotorPalette──────────────> C0
  └─ ReferenceObservationPoints─────> L9

A0 LocalMotorToMatrix
  └─ LocalMatrixPalette─────────────> A1

A1 MatrixHierarchy
  └─ GlobalMatrixPalette────────────> A2

A2 MatrixProbeApply
  └─ MatrixObservationPoints────────> L9

B0 MotorHierarchy
  └─ GlobalMotorPaletteB────────────> B1

B1 DirectPgaProbeApply
  └─ PgaObservationPoints───────────> L9

C0 MotorHierarchy
  └─ GlobalMotorPaletteC────────────> C1

C1 GlobalMotorToMatrix
  └─ GlobalMatrixPaletteC───────────> C2

C2 MatrixProbeApply
  └─ HybridObservationPoints────────> L9

L9 HierarchyPointComparison
  └─ ComparisonRecords──────────────> Readback
```

合計：

```text
10 Leaves
11前後のinternal Buffer Flows
single writer per Flow
static same-frame DAG
one DeviceDomain
headless
whole-composition recovery
```

現在のLevel 4 v1が証明しているのは、Buffer-onlyの有限静的same-frame DAG、single writer、単一DeviceDomain、whole-composition recoveryである。

# 8. Completion Unit構成

Spiral 2は7個のCompletion Unitで完成させる。

```text
CU0 Research Contract Freeze
CU1 Hierarchy Semantic and Dynamic Invocation
CU2 Candidate Graph and Independent Authority
CU3 Frozen Leaf Groups
CU4 Level 4 Composition Sufficiency
CU5 WARP, Determinism and Recovery
CU6 Measurement, Decision and Final Freeze
```

# CU0：Research Contract Freeze

## Stage S2-00：Decision Closure

作業：

```text
Spiral 1のNextCapabilitySelectionを
FixedDynamicMotorPaletteAndHierarchyへ確定

Spiral 1 Experiment Complete bannerを閉じる

Spiral 2 milestone identityを固定
```

完成条件：

```text
Spiral 1 Decision Report
Spiral 2 Selection Record
次能力が一意
```

## Stage S2-01：Spiral 2 Completion Specification

C++変更前に次を固定する。

```text
対象範囲
非目標
Semantic schema
Dynamic Invocation schema
三候補のLeaf graph
Probe schema
Observation Contract
Corpus
Authority chain
Negative Gate
Recovery contract
Completion Invariants
Stage map
```

完成条件：

> コードを一行も追加せず、何を入力し、何を生成し、どこで拒否し、何をもって完成とするかを一意に説明できる。

## Stage S2-02：Foundation Baseline Freeze

次を固定する。

```text
afaed65d... source inventory
Spiral 1 Architecture evidence
Level 4 v1 evidence
Schema 17 / Runtime 17
Project dependency graph
既存test runner
```

Spiral 2の追加によって、Spiral 1およびLevel 4 v1の証明を変更していないことを確認する。

# CU1：Hierarchy Semantic and Dynamic Invocation

## Stage S2-03：Canonical Hierarchy Semantic

実装：

```text
Hierarchy Builder
Parent table validation
Cycle rejection
Single-root validation
Canonical bone order
Depth-layer derivation
Serialization
Semantic digest
```

Negative Gate：

```text
Bone count 0
parent範囲外
self parent
cycle
multiple root
非canonical order
depth table改変
topology identity差替え
```

完成条件：

```text
同じTopologyから
Debug / Release / fresh processで
byte-identical Semanticを生成
```

## Stage S2-04：Dynamic Motor Palette Authority

実装：

```text
DynamicLocalMotorPaletteV1
Dynamic Palette Builder
Invocation schema
exact-byte validation
canonical float normalization
```

必須証明：

```text
Palette値を変えてもFrozen成果物は変わらない
Bone countを変えると再Compileが必要
Topologyを変えると再Compileが必要
同じFrozen Compositionへ連続した異なるPaletteをSubmitできる
```

ここはSpiral 2の最重要境界の一つである。

## Stage S2-05：Corpus and CPU Reference

固定Corpusを作る。

### Topology corpus

```text
H00  1 bone root
H01  2 bone chain
H02  8 bone star
H03  8 bone chain
H04  15 bone balanced tree
H05  32 bone mixed tree
H06  64 bone balanced tree
H07  64 bone chain
H08  128 bone mixed tree
```

### Dynamic frame corpus

```text
F00 all identity
F01 translations only
F02 same-axis rotations
F03 alternating XYZ rotations
F04 rotation + translation
F05 very small angles
F06 angles near π
F07 fixed-seed random local Motors
F08 deep-chain accumulated rotation
F09 deep-chain accumulated translation
F10 mixed magnitude coordinates
F11 canonical-sign boundary cases
```

すべて固定seed、固定順序、固定bytesとする。

完成条件：

```text
Topology
Invocation
Reference outputs
Observation identity
```

がfresh process間で決定的。

# CU2：Candidate Graph and Independent Authority

## Stage S2-06：Three Candidate Graph Generation

同じSemantic identityから、次の三つを生成する。

```text
MatrixHierarchyGraphV1
DirectPgaHierarchyGraphV1
HybridHierarchyGraphV1
```

Candidate identityには少なくとも次を含む。

```text
Semantic identity
Candidate graph kind
Leaf role sequence
Flow topology
Lowering versions
Program template identities
Binding layout digests
Dynamic input schema
Intermediate schemas
Observation Contract
Target profile
```

完成条件：

> 同じ意味から、Leaf数・Flow・中間表現・Matrix化位置が異なる三候補を生成できる。

## Stage S2-07：Independent Hierarchy Verifier

VerifierはPlanner実装を呼び直してはならない。

再導出するもの：

```text
Parent table
Depth layers
Leaf role sequence
Flow graph
Matrix化位置
Dynamic input schema
Intermediate stride
Program template digest
Binding layout digest
Observation output schema
```

候補固有検査：

### Matrix

```text
Motor→MatrixがHierarchy前
Matrix hierarchy template
48-byte palette stride
```

### Direct PGA

```text
Matrix materialization Leafが存在しない
Motor hierarchy template
32-byte palette stride
Direct probe apply template
```

### Hybrid

```text
Motor hierarchy後にだけMatrix化
Global Motor→Matrix template
Matrix probe apply template
```

完成条件：

```text
Raw Candidate GraphはFreeze不能
Raw Candidate GraphはComposition登録不能
Verifier sealなしではLeaf Compile不能
```

## Stage S2-08：Authority Mutation Suite

拒否対象：

```text
parent index改変
depth layer改変
Candidate graph kind偽装
A/B/CのLeaf順序交換
Matrix化位置交換
Intermediate stride改変
Program template差替え
Dynamic Slot identity差替え
Bone count差替え
Observation Contract差替え
Raw seal replay
別Semanticのseal流用
Target profile差替え
```

# CU3：Frozen Leaf Groups

## Stage S2-09：Dynamic Source Vertical Slice

最初に、

```text
2 bones
2 frames
1 candidate branch
```

だけでDynamic Invocationを通す。

目的はHierarchy性能ではなく、

```text
FrameInvocation
→ Leaf dynamic data
→ Composition Flow
```

が現在のSchema 17／Runtime 17／Level 4 v1で閉じるかを判定すること。

### 成功した場合

Level 4 ABIを変更せず続行する。

### 失敗した場合

一時的なRuntime hackを追加しない。

次を記録する。

```text
既存契約で表せない正確な事実
不足する所有権
必要なFrozen情報
Runtimeが推測してしまう箇所
Recovery時の問題
Negative scenario
```

その後だけ、最小のLevel 4拡張仕様へ進む。

## Stage S2-10：Matrix Leaf Group

資格化：

```text
A0 LocalMotorToMatrix
A1 MatrixHierarchy
A2 MatrixProbeApply
```

必須条件：

```text
CPUでMatrixを事前生成しない
同じMotor input bytesを使用
Leaf-only WARP observation pass
Source / Plannerなしで再物質化可能
```

## Stage S2-11：Direct PGA Leaf Group

資格化：

```text
B0 MotorHierarchy
B1 DirectPgaProbeApply
```

必須条件：

```text
Matrix constantなし
Matrix intermediateなし
隠れたMatrix展開なし
CPU reference契約を満たす
```

## Stage S2-12：Hybrid Leaf Group

資格化：

```text
C0 MotorHierarchy
C1 GlobalMotorToMatrix
C2 MatrixProbeApply
```

必須条件：

```text
Hierarchy前にMatrix化しない
Global Motor Bufferを実際に生成
Matrix化境界がVerifierで固定
```

# CU4：Level 4 Composition Sufficiency

## Stage S2-13：Ten-Leaf Frozen Comparison Composition

三候補を一つのCompositionへ接続する。

検査：

```text
全internal Flowがsingle writer
LocalMotorPaletteはfan-out read
ReferenceはComparison Leafだけが読む
候補間でIntermediate Bufferを共有しない
Graphはfinite static DAG
Presenterは0
DeviceDomainは1
Recovery unitは全体
```

## Stage S2-14：Level 4 Sufficiency Gate

ここで正式に判定する。

### 判定A：Level 4 v1で十分

```text
No Level 4 extension
Level 5側の複数Leaf群だけで成立
```

### 判定B：最小Level 4拡張が必要

想定される不足候補は、

```text
Composition-level Dynamic Invocation mapping
Leaf Dynamic SlotからShared Flowへの明示接続
Invocation generation identity
連続frame input ownership
```

ただし、これらを先に実装してはならない。

失敗証拠が得られた項目だけをLevel 4 vNextへ追加する。

### Canonical Reconstruction条件

Level 4変更が必要な場合は、

```text
Discovery implementation
        ↓
必要性・不変条件・拒否条件確定
        ↓
Frozen representation確定
        ↓
Runtime / Recovery意味確定
        ↓
Canonical Reconstruction
        ↓
Spiral 2を再接続
```

とする。

探索コードをそのままCanonical Level 4へ昇格させない。

# CU5：WARP、決定性、Recovery

## Stage S2-15：WARP Hierarchy Corpus

H00～H08とF00～F11の組合せを実行する。

少なくとも次を満たす。

```text
全候補 vs CPU reference
A vs B
A vs C
B vs C
non-finite = 0
mismatch = 0
rigidity contract pass
```

性能優位はGateにしない。

## Stage S2-16：Multi-frame Dynamic Invocation

同じFrozen Compositionに対して、

```text
Frame F00
Frame F01
Frame F02
...
```

を連続Submitする。

証明すること：

```text
Palette変更で再Compileしない
Frozen bytesは不変
出力はInvocationに応じて変化
前frame値が混入しない
frame順序を入れ替えると対応する出力へ変わる
```

Temporal historyは持たないため、同じInvocation bytesならframe番号に関係なく同じ結果を要求する。

## Stage S2-17：Determinism Freeze

次を比較する。

```text
Debug fresh process A
Debug fresh process B
Release fresh process
```

byte-identical対象：

```text
Hierarchy Semantic
Topology manifest
Candidate Graph manifests
Verified Representation Plans
Frozen Leaf Packages
Composition Contract
Verified Composition Plan
Frozen Composition
Corpus definitions
Observation Contract
```

Dynamic frame valuesはFrozen成果物比較の入力に含めない。

## Stage S2-18：Recovery Qualification

必須scenario：

```text
Load Frozen Composition
Submit F04
Readback
Controlled recovery
Submit same F04
Compare

Submit F07
Actual RemoveDevice
Exclude removed LUID
AwaitingAdapter or rematerialize
fresh processで同じFrozen artifactをload
Submit same F07
Compare
```

必須拒否：

```text
old shared Buffer
old completion token
old Dynamic Slot binding
old invocation submission
stale device epoch
```

Recovery後、動的Paletteは再bindが必要であり、古いInvocationを暗黙に再利用してはならない。

# CU6：Architecture Freeze、実測、Decision

## Stage S2-19：Architecture Final Freeze

Architecture Complete条件：

```text
S2-I01～S2-I22合格
三候補生成
独立Verifier合格
Raw Candidate非実行
三つのFrozen Leaf群
Level 4 Frozen Composition
全WARP corpus合格
Multi-frame Invocation合格
Debug / Release determinism合格
Controlled / actual recovery合格
```

推奨banner：

```text
============================================================
SGE4-5 SPIRAL 2 HIERARCHICAL REPRESENTATION AUTHORITY PASSED
Canonical meaning: fixed hierarchy with dynamic local PGA Motor palette
Candidates: Matrix hierarchy / Direct PGA hierarchy / Hybrid hierarchy
Authority: raw candidate graphs are non-executable
Composition: verified multi-Leaf Frozen Composition
Invocation: per-frame Motor values remain outside Frozen identity
Observation: common per-bone probe contract qualified
Recovery: dynamic rebind and whole-composition rematerialization qualified
============================================================
```

## Stage S2-20：Real GPU Measurement

測定ケースは少なくとも次とする。

```text
M0  8 bones, star
M1  15 bones, balanced
M2  32 bones, mixed
M3  64 bones, balanced
M4  64 bones, chain
M5  128 bones, mixed
```

記録項目：

```text
Candidate generation time
Verifier time
Freeze time
Package bytes
Dynamic upload bytes
Intermediate allocation bytes
Materialization time
Command recording time
GPU time per Leaf
GPU total time
End-to-end time
Dispatch count
Barrier count
Numeric error
```

三候補の実行順序は、全6順列を均等に使う。

```text
ABC
ACB
BAC
BCA
CAB
CBA
```

これを反復し、順序biasを抑える。

同じHardware adapter、driver、clock policy、Topology、Invocation、Queue、timestamp frequencyを使う。

## Stage S2-21：Decision Evidence Report

必ず答える問い：

Level 4 v1だけでDynamic Palette Compositionを閉じられたか。

閉じられなかった場合、正確にどのFrozen契約が不足したか。

毎frameの値をFrozen identityから完全に分離できたか。

同じ意味から、実際に異なるLeaf群を生成できたか。

Matrix化をHierarchy前、Hierarchy後、行わない場合で何が変わったか。

Bone数KとHierarchy depth Cによって優位候補が変わったか。

Motor 32 bytesとMatrix 48 bytesの中間Palette差は実測へ影響したか。

Direct PGAの演算増加とBuffer削減のトレードオフはどうなったか。

Hybridが両方式の長所を利用できたか。

VerifierはLeaf graphおよびLowering境界の改変を検出できたか。

次に必要なのはTemporal Flow、可変Work、Vertex Consumer、Variant Setのどれか。

Level 4へ追加する能力は、本当に実験から要求されたか。

## Stage S2-22：Spiral 2 Experiment Complete

完成条件：

```text
Architecture Complete
+
実GPU measurement
+
Decision Evidence Report
+
次のSpiralまたはLevel 4拡張を一つ選択
```

推奨banner：

```text
============================================================
SGE4-5 SPIRAL 2 EXPERIMENT COMPLETE
Dynamic hierarchical representation composition completed.
Matrix, Direct PGA and Hybrid Leaf groups were fairly compared.
No performance superiority is implied by completion.
The next Spiral is selected from observed Level 4 and Level 5 evidence.
============================================================
```

# 9. Completion Invariants

| ID | 不変条件 |
| --- | --- |
| S2-I01 | Canonical Hierarchy SemanticにMatrix、Queue、Shaderを含めない |
| S2-I02 | Bone数とTopologyはFrozen identityへ含まれる |
| S2-I03 | 毎frameのMotor値はFrozen identityへ含まれない |
| S2-I04 | 同じFrozen Compositionへ異なるPaletteをSubmitできる |
| S2-I05 | 三候補は同一のMotor input bytesを読む |
| S2-I06 | CPU側で候補A専用Matrix Paletteを生成しない |
| S2-I07 | 同じSemanticから三つのCandidate Graphを生成できる |
| S2-I08 | Candidate GraphごとにLeaf数、Flow、Lowering位置が固定される |
| S2-I09 | Raw Candidate GraphはFreezeも実行もできない |
| S2-I10 | VerifierがHierarchy topologyを独立再導出する |
| S2-I11 | Verifierが候補ごとのMatrix化位置を独立検証する |
| S2-I12 | 三候補のObservation schemaが同一である |
| S2-I13 | 全internal Flowがexactly one writerを持つ |
| S2-I14 | Runtimeは候補、schedule、allocation、state、syncを再判断しない |
| S2-I15 | Dynamic inputの変更だけでは再Compileしない |
| S2-I16 | TopologyまたはBone数変更では別Frozen identityになる |
| S2-I17 | 全WARP corpusがCPU Reference契約を満たす |
| S2-I18 | 三候補間の相互差が誤差契約を満たす |
| S2-I19 | 同じInvocationはfresh process間で同じreadbackを生成する |
| S2-I20 | Frozen成果物がDebug／Release／fresh processでbyte-identical |
| S2-I21 | Recovery後に動的Paletteの再bindを要求する |
| S2-I22 | stale epochのBuffer、token、Invocationを拒否する |
| S2-I23 | 公平な三候補実GPU reportを生成する |
| S2-I24 | 次の能力はDecision Evidenceなしに追加されない |

# 10. 推奨Project構成

既存の60～79番台がSpiral 1なので、Spiral 2は80番台以降へ分ける。

```text
80_HierarchySemantic
81_Spiral2Contracts
82_Spiral2Corpus
83_HierarchyRepresentationCandidate
84_HierarchyRepresentationPlanner
85_HierarchyRepresentationVerifier
86_Spiral2LeafCompiler
87_Spiral2Observer
88_Spiral2Scenarios
89_Spiral2PerformanceExperiment

90_Spiral2SemanticTests
91_Spiral2DynamicInvocationTests
92_Spiral2RepresentationPlanningTests
93_Spiral2RepresentationAuthorityTests
94_Spiral2LeafPackageTests
95_Spiral2CompositionTests
96_Spiral2WarpObservationTests
97_Spiral2RecoveryTests
98_Spiral2FreezeTests
99_Spiral2Launcher
```

依存方向：

```text
Hierarchy Semantic
        ↓
Candidate Planner
        ↓
Raw Candidate Graph
        ↓
Independent Verifier
        ↓
Verified Representation Graph
        ↓
Leaf Compiler
        ↓
Frozen Leaf Packages
        ↓
Level 4 Composition
        ↓
Frozen Runtime
```

禁止依存：

```text
Verifier → Planner implementation
Runtime → Hierarchy Semantic
Backend → Hierarchy Semantic
Backend → Representation Planner
Frozen Leaf → Source Semantic
Candidate branch A → Candidate branch B/C
```

# 11. 実装上の最大リスク

## Risk 1：Dynamic input権威

最も重要なリスク。

```text
動的値をPackageへ固定しすぎる
または
Runtimeへ数学判断を漏らす
```

危険がある。

対策：

```text
Semantic validationはInvocation Builder
Runtimeはschema validation
Frozenはschemaだけを保持
```

と分離する。

## Risk 2：Matrix候補の不公平

CPUでMatrix Paletteを生成してMatrix候補だけへ渡すと、PGA候補との比較条件が崩れる。

全候補は同じMotor bytesを受け取り、Matrix化も候補のGPU Workに含める。

## Risk 3：Hierarchyアルゴリズム差

MatrixとPGAでまったく異なるHierarchy走査を使うと、表現差とアルゴリズム差を分離できない。

最初の版では、

```text
同じcanonical depth layers
同じBone order
同じdispatch partition
```

を使う。

## Risk 4：Level 4の先回り拡張

Dynamic Composition inputが必要そうだという推測だけでABIを追加してはならない。

まずDynamic Source Vertical Sliceを実行し、既存能力で不可能であることをNegative Gateで示す。

## Risk 5：範囲肥大

Skinning、Temporal、可変countを同時に入れると、失敗原因を切り分けられない。

Spiral 2ではHierarchy global transformの生成と共通Probe観測までで止める。

# 12. 最短のCritical Path

完成へ必要な最短経路は次である。

```text
S2-00～02
Research ContractとBaselineを固定
        ↓
S2-03～05
Hierarchy Semantic、Dynamic Invocation、Reference
        ↓
S2-06～08
三Candidate Graphと独立Verifier
        ↓
S2-09
Dynamic inputがLevel 4へ通るか最小実証
        ↓
S2-10～12
Matrix / PGA / Hybrid Leaf群
        ↓
S2-13～14
Frozen CompositionとLevel 4 Sufficiency判定
        ↓
S2-15～18
WARP、multi-frame、determinism、Recovery
        ↓
S2-19
Architecture Complete
        ↓
S2-20
実GPU測定
        ↓
S2-21～22
Decision Report、次能力選択、Experiment Complete
```

この順序では、Dynamic input境界が成立する前に三候補全部を作らない。

最大の不確実性を先に小さなVertical Sliceで潰す。

# 13. Spiral 2の到達点

完成すると、SGE4-5は次へ進む。

Spiral 1：

```text
一つの固定数学的意味
→ 二つのLeaf実装候補
```

Spiral 2：

```text
固定構造
+
毎frame変わる数学的入力
→ 三つの異なるLeaf群候補
→ 異なる中間表現
→ 異なるLowering位置
→ 一つのLevel 4 Frozen Composition
```

これは、Level 5が単一Leaf選択からPackage群生成へ進み、同時にLevel 4が動的入力を含む現実的なLeaf群を固定できるか試される段階である。

最終的な意味は、

> Level 5が複数のLeaf群を作り、その圧力によってLevel 4の次の必要能力を実証的に発見する、二回目の正式な螺旋

である。

---

Source file SHA-256: `c1f1210a5062e43e97a08ed5931192d2778bf72e35a2de3741de6a46646828d5`
This file is a format conversion of the supplied DOCX. Process overrides are only in `00_OWNER_OVERLAY.md`.
