# Level 4 Generalization 3 — Offscreen Raster Semantic Contract 修正記録

## 1. 失敗

Windows統合設計試験で、限定Texture2D Compositionの実Leaf生成が次の段階で拒否された。

```text
semantic-analysis：Workが検証または実行の契約に違反しています。
```

## 2. 原因

`ColorAttachment`のResourceUse検証は、Generalization 3で固定External Texture2DをRenderTargetとして許可していた。

一方、Raster Work全体の検証には従来のPresentation専用条件が残り、すべてのRasterへ次を要求していた。

```text
VertexData       1
ColorAttachment  1
PresentSource    1
```

そのため、Surfaceを持たずExternal Texture2Dへ描画する正しいoffscreen Rasterが、ResourceUse境界では受理されながらWork境界で拒否されていた。

## 3. 修正

Raster WorkをColorAttachmentのResource kindによって次の二契約へ分離した。

### Presentation Raster

```text
ColorAttachment resource = SurfaceImage
PresentSource count       = 1
PresentSource resource    = ColorAttachmentと同一
```

### Limited Offscreen Raster

```text
ColorAttachment resource = fixed External Texture2D
PresentSource count       = 0
```

共通条件は維持する。

```text
VertexData count       = 1
ColorAttachment count  = 1
DepthAttachment count <= 1
PresentSource count   <= 1
Copy operand count     = 0
```

## 4. 変更しない境界

- Surface RasterのPresent必須条件
- PresentSourceがSurfaceImageだけを参照できる条件
- 限定Texture2Dのformat／extent／mip条件
- SGE4UNI 2.3
- Leaf Schema 17
- Composition Planner／Verifier
- Texture state／completion handoff
- D3D12物質化、readback、Recovery

## 5. 回帰確認

PortableなSemantic Analysisで次を確認した。

```text
External Texture2D ColorAttachment + no PresentSource : accepted
SurfaceImage ColorAttachment + matching PresentSource : accepted
SurfaceImage ColorAttachment + no PresentSource       : rejected
```

Windows Full Gateで、通常のSemantic Compilerからproducer／consumer Raster Leafを生成し、Texture2D Composition、HLSL、WARP、readback、Recoveryを最終確認する。
