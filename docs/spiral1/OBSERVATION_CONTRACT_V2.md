# Spiral 1 Observation Contract V2

- Contract ID: `SGE4-5.Spiral1.Observation.V2`
- Status: Active; supersedes V1 pass metric with recorded S12 counterexample
- Record layout: `PointComparisonRecordV1` remains 96 bytes
- CPU reference algorithm: unchanged binary64 direct Motor evaluation, rounded once to binary32

## 1. Why V1 was insufficient

V1 scaled each XYZ tolerance by the magnitude of that same output component. S12 contains input coordinates near 10000 and a rigid rotation whose large terms cancel in one output component. A representative deterministic case produced a reference component near -6.774 while the binary32 Matrix/PGA paths differed by about 2.46e-4. V1 allowed only about 1.68e-4 for that component, even though the error was the expected forward error of float32 coefficients applied to a point whose vector scale was several thousand.

This is a conditioning defect in the V1 pass metric, not evidence that the Matrix or Direct PGA lowering changed the represented rigid transform.

## 2. V2 reference scale

For one reference point `r`:

```text
referencePointScale = max(abs(r.x), abs(r.y), abs(r.z), 1.0)
```

For every XYZ component:

```text
abs(actualComponent - referenceComponent)
 <= absoluteTolerance
  + relativeTolerance * referencePointScale
```

The numeric values remain:

```text
absoluteTolerance = 1.0e-4
relativeTolerance = 1.0e-5
```

A rigid rotation mixes all input components. Therefore its forward-error scale is the point/vector magnitude, not the magnitude of an individual component after cancellation.

## 3. V2 pair scale

```text
matrixPointScale = max(abs(matrix.x), abs(matrix.y), abs(matrix.z), 1.0)
pgaPointScale    = max(abs(pga.x), abs(pga.y), abs(pga.z), 1.0)
pairPointScale   = max(matrixPointScale, pgaPointScale)
```

For every XYZ component:

```text
abs(matrixComponent - pgaComponent)
 <= pairAbsoluteTolerance
  + relativeTolerance * pairPointScale
```

`pairAbsoluteTolerance` remains `1.0e-4`.

## 4. Unchanged properties

- S00-S15 input points, motors, seeds and binary64 CPU references are unchanged.
- `PointComparisonRecordV1` layout and mismatch bits are unchanged.
- GPU and CPU still generate byte-identical records.
- non-finite values and invalid `w` remain unconditional failures.
- thresholds remain part of the Observation Contract identity and cannot be changed after Freeze.
- V1 remains available as historical evidence and as the explicit negative control for the S12 conditioning counterexample.

## 5. Promotion rule

V2 is accepted only because the frozen S12 corpus produced a concrete reproducible V1 false negative. It does not widen tolerances by changing their numeric values and does not remove any corpus point. The scale algorithm alone is superseded.
