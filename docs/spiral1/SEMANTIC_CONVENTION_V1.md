# Spiral 1 Semantic Convention V1

- Contract ID: `SGE4-5.Spiral1.SemanticConvention.V1`
- Status: Frozen

## 1. 対象

正規化された一つの3D rigid transformを、有限個のEuclidean pointへactive transformとして適用する。General PGA multivector systemは対象外である。

## 2. 座標と積

- 基底は`e1, e2, e3`で、`e1 cross e2 = e3`。
- camera、screen、left-handed／right-handed API conventionはSemanticへ持ち込まない。
- quaternion component orderは`(w, x, y, z)`。
- Hamilton productを用いる。
- point quaternionは`p = (0, px, py, pz)`。
- active rotationは`qr * p * conjugate(qr)`。
- translationはrotation後に加える。

## 3. Canonical Motor

```text
Motor = qr + epsilon qd
qd = 0.5 * (0, tx, ty, tz) * qr
```

translation extractionは次である。

```text
t = vector_part(2 * qd * conjugate(qr))
```

公開Builderは`BuildIdentityMotor()`と`BuildMotor(UnitQuaternion, Float3d)`だけを提供する。raw 8係数BuilderはNegative test内部に限定する。

## 4. Numeric domain

- Canonical coefficients: IEEE-754 binary64
- GPU point record: IEEE-754 binary32 `Float4Point`
- 全入力はfinite
- point `w`はbit-exact `0x3F800000`
- qualification範囲: `abs(x|y|z) <= 10000`
- point count: `1..4096`、S15は各subscenario 256

## 5. Quaternion normalization

Builderはbinary64でnormを計算し、zero、subnormal-only、NaN、Infinityを拒否して正規化する。

受理前条件:

```text
normSquared is finite
normSquared >= 2^-40
```

正規化後、`abs(normSquared - 1.0) <= 2^-48`をCanonical validationで要求する。

## 6. Canonical sign

`qr`と`-qr`を一つへ統一する。`qr`を`(w,x,y,z)`順に走査し、最初に`abs(component) > 2^-52`となる成分を正にする。反転時は`qd`も同時反転する。

全`-0.0`はserialization前に`+0.0`へ正規化する。

## 7. Canonical record

```text
PgaMotorSemanticV1
  conventionVersion : u32 = 1
  reserved          : u32 = 0
  qr[4]             : binary64 bits, wxyz
  qd[4]             : binary64 bits, wxyz

PointBatchSemanticV1
  elementKind       : u32 = 1 (Float4Point)
  strideBytes       : u32 = 16
  pointCount        : u32
  reserved          : u32 = 0
  corpusIdentity    : Digest256

ApplyPgaMotorSemanticV1
  motorIdentity
  pointBatchIdentity
  outputSchemaIdentity
  observationContractIdentity
```

Canonical SemanticにMatrix、HLSL、Queue、Descriptor、Buffer state、Dispatch countを置いてはならない。

## 8. Serialization

- little endian
- fixed-width fields
- field order固定
- reservedは0
- NaN／Infinity拒否
- `-0.0`正規化
- timestamp、path、pointer、UUID、process固有値禁止

## 9. Semantic identity

次のcanonical bytesを順にhashする。

```text
semantic schema identity
motor canonical bytes
point schema identity
point count
corpus identity
output schema identity
observation contract identity
```

Lowering kind、Target profile、Program digestは含めない。

## 10. Negative Gate

zero quaternion、non-finite、non-canonical sign、unknown convention、invalid point `w`、point count 0／上限超過、reserved非0、Observation Contract欠落を拒否する。
