# Spiral 1 Observation Contract V1

- Contract ID: `SGE4-5.Spiral1.Observation.V1`
- Status: Frozen

## 1. 比較対象

各point indexについて次の三比較を行う。

1. Matrix output vs CPU binary64 reference rounded to binary32
2. Direct PGA output vs同reference
3. Matrix output vs Direct PGA output

point count、順序、schemaは全経路で同一でなければならない。

## 2. CPU reference

CPU referenceはCanonical Motorからbinary64で直接評価する。Matrix candidate実装、Direct PGA shader移植、candidate planner helperを呼んではならない。

```text
rotated = qr * pointQuaternion * conjugate(qr)
translation = 2 * qd * conjugate(qr)
result = rotated.xyz + translation.xyz
```

reference pointはround-to-nearest-ties-to-evenでbinary32へ一度だけ丸め、`w=1.0f`とする。

## 3. Tolerance

```text
absoluteTolerance     = 1.0e-4
relativeTolerance     = 1.0e-5
pairAbsoluteTolerance = 1.0e-4
```

各XYZ成分のreference比較は次を満たす。

```text
abs(actual - reference)
 <= absoluteTolerance
  + relativeTolerance * max(abs(reference), 1.0)
```

pair比較にも同じ式を使い、右辺のreference項には`max(abs(matrix), abs(pga), 1.0)`を使う。non-finiteは無条件fail。

## 4. ULP distance

float bit patternを次のordered integerへ写像する。

```text
if sign bit is set: ordered = 0x80000000 - bits
else:               ordered = 0x80000000 + bits
ulp = abs(orderedA - orderedB)
```

`+0.0`と`-0.0`はdistance 0として扱う。NaN／InfinityはULP比較前にnon-finite failとする。ULP値は診断であり、V1 pass/failの独立閾値には使わない。

## 5. PointComparisonRecordV1

96-byte fixed record、little endian。

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | pointIndex u32 |
| 4 | 4 | finiteFlags u32 |
| 8 | 12 | matrixAbsErrorXYZ binary32 |
| 20 | 12 | pgaAbsErrorXYZ binary32 |
| 32 | 12 | pairAbsErrorXYZ binary32 |
| 44 | 12 | matrixUlpXYZ u32 |
| 56 | 12 | pgaUlpXYZ u32 |
| 68 | 12 | pairUlpXYZ u32 |
| 80 | 4 | mismatchFlags u32 |
| 84 | 12 | reserved = 0 |

`finiteFlags` bits: 0 reference finite, 1 matrix finite, 2 PGA finite, 3 matrix w exact, 4 PGA w exact。

`mismatchFlags` bits:

```text
0 reference non-finite
1 matrix non-finite
2 PGA non-finite
3 matrix w invalid
4 PGA w invalid
8..10  matrix XYZ tolerance failure
12..14 PGA XYZ tolerance failure
16..18 pair XYZ tolerance failure
```

その他bitは0。

## 6. Deterministic aggregation

GPU上でatomic reductionを行わない。CPU Observerはpoint index昇順でbinary64 accumulatorを用いて集計する。RMSは各pointのXYZ squared errorをその順で加算して最後にsqrtする。

各scenario reportは次を持つ。

```text
scenario identity
point count
non-finite count
mismatch count
first mismatch index
max absolute error
max relative error
max ULP distance
RMS Euclidean error
input/reference/output/readback digests
```

## 7. Determinism scope

同一WARP、同一Frozen artifact、同一inputではComparison Record bytesをfresh process間で一致させる。異なるphysical adapter／driver間はbyte一致を要求せず、Tolerance passを要求する。

## 8. Contract identity

Tolerance、record layout、flag layout、reference algorithm version、aggregation orderをcanonical encodeしたdigestをObservation Contract identityとする。実行後の閾値変更は禁止する。
