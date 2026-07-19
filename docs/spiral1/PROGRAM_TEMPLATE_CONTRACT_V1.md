# Spiral 1 Canonical Program Template Contract V1

- Contract ID: `SGE4-5.Spiral1.ProgramTemplates.V1`
- Status: Frozen

## 1. 信頼境界

Spiral 1はarbitrary HLSL equivalence proverを作らない。Representation Verifierはversioned Canonical Program Templateのidentity、source digest、compiled binary digest、reflection interface digestをallowlistで検査する。

## 2. 共通Target profile

```text
Shader compiler: D3DCompile
Shader profile: cs_5_1
Entry point: CSMain
Compile flags:
  D3DCOMPILE_ENABLE_STRICTNESS
  D3DCOMPILE_WARNINGS_ARE_ERRORS
  D3DCOMPILE_IEEE_STRICTNESS
  D3DCOMPILE_OPTIMIZATION_LEVEL3
Thread group: [numthreads(64,1,1)]
```

Compiler DLL identityとbinary digestはTarget profile／Provenanceへ記録する。Program binaryが変わる場合はCandidate identityも変わる。

## 3. Common interface

```text
t0: StructuredBuffer<float4> InputPoints
u0: RWStructuredBuffer<float4> OutputPoints
b0: Template-specific constants
```

- input stride 16
- output stride 16
- point countはb0にu32で固定
- dispatchX = ceil(pointCount / 64)
- thread index >= pointCountは何も書かずreturn
- output順はinput順
- output.wはliteral `1.0f`
- UAV bounds外書込禁止

## 4. MatrixExpandedComputeV1

Constant payloadは64 bytes。

```text
0..47   12 binary32: row-major 3x4 affine matrix
48..51  pointCount u32
52..63  reserved = 0
```

式:

```text
out.x = m00*x + m01*y + m02*z + tx
out.y = m10*x + m11*y + m12*z + ty
out.z = m20*x + m21*y + m22*z + tz
out.w = 1.0f
```

MatrixはCanonical binary64 MotorからCompilerがbinary64で導出し、各係数をbinary32へ一度だけ丸める。

## 5. DirectPgaMotorComputeV1

Constant payloadは48 bytes。

```text
0..31   qr.wxyz, qd.wxyz: 8 binary32
32..35  pointCount u32
36..47  reserved = 0
```

Shaderは8係数からquaternion sandwichとtranslation extractionを直接評価する。Matrix、3x3 basis、hidden expanded constantsを持ってはならない。

## 6. Interface equivalence

二Templateは同じinput/output schema、point count、thread group size、Buffer state要求、Observation Contract identityを持つ。異なるのはProgram Template identityとconstant payloadだけである。

## 7. Verifier obligations

Verifierは次を検査する。

- exact template ID／version
- exact constant byte count／reserved zero
- source digest、compiled binary digest、reflection digest
- t0/u0/b0 register contract
- point countとSemantic schema一致
- Matrix constantまたはMotor constantの独立再導出
- Observation Contract identity

## 8. Forbidden substitutions

entry point、profile、compile flag、register、stride、thread group、rounding order、constant layout、output.w、guard条件を黙って変更してはならない。変更はV2とする。
