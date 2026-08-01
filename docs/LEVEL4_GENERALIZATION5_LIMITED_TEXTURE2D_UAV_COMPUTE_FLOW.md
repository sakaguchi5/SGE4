# Level 4 Generalization 5 — Limited Texture2D UAV／Compute Flow

## 1. 目的

Generalization 5は、Generalization 3で正本化した限定Texture2D Flowを、RasterTarget writerだけでなくCompute UAV writerへ拡張する。

Runtimeへ任意のTexture usage policyを追加することは目的ではない。埋込みSchema 17 Leaf Packageが所有するResource kind、format、view、required state、guaranteed stateから、Composition Modelが次の一つの限定経路を再導出する。

```text
Compute Leaf
  RWTexture2D<float4> / UAV / R32G32B32A32_FLOAT
        ↓ UnorderedWrite completion
Composition-owned shared Texture2D
        ↓ verified state handoff
Raster Consumer Leaf
  Texture2D<float4> / SRV / PixelShaderRead
        ↓
BGRA8 Composition Output
        ↓
packed readback
```

## 2. Frozen形式

Production Frozen Compositionは`SGE4UNI 2.5`である。

```text
SGE4UNI 2.5
  Manifest schema 2
  Leaf Table schema 1
  complete Schema 17 Leaf bytes
  Contract Data schema 2
  Verified Decision Data schema 2
  Verification Certificate schema 1
  Authority Ledger schema 2
  Dynamic Contract schema 4
```

Contract／Planのbinary record layoutは2.4から変更しない。既存の`format`、`Texture2DFlowShape`、endpoint state、handoff stateで限定UAV経路を完全に表現できるためである。

Frozen Dynamic Invocationは`SGE4INV 1.4`、Leaf PackageはSchema 17を維持する。

## 3. 対象Texture

Generalization 5のCompute writable Textureは次に限定する。

```text
Resource kind      Texture2D
Format             R32G32B32A32_FLOAT
Extent             fixed width／height
rowBytes           width * 16
Mip                1
Array layer        1
Plane              1
Sample count       1
Writer             exactly one
Writer view        UnorderedAccess
Writer state       UnorderedWrite
Reader view        ShaderResource
Reader state       PixelShaderRead または NonPixelShaderRead
時間関係           same-frame static DAG
```

BGRA8 Texture2Dは従来どおりRenderTarget writer専用である。R32G32B32A32_FLOAT Texture2Dは今回の限定UAV writer専用である。

これにより、formatとstateからwrite roleを一意に再導出できる。

```text
BGRA8 + RenderTarget      -> Raster writer
RGBA32F + UnorderedWrite  -> Compute UAV writer
```

## 4. Semantic Compiler

Semantic Modelへ次を追加する。

```text
FormatMeaning::Rgba32Float
ViewRole::StorageTexture2D
ProgramParameterKind::UnorderedTexture2D
```

`StorageTexture2D`は次を要求する。

- External fixed Texture2D
- Rgba32Float
- mip 1
- Write-only use
- Compute stage
- UAV register

Shader Reflectionは`D3D_SIT_UAV_RWTYPED`かつ`TEXTURE2D`を`UnorderedTexture2D`として独立に照合する。Buffer UAVとTexture UAVを同じResource kindとして扱わない。

## 5. PackageとComposition authority

Schema 17 PackageはExternal Texture2D UAV viewとUnorderedWrite stateを所有する。Composition ModelはPackageをdecodeし、次を再導出する。

- Texture format
- fixed extent
- packed rowBytes
- write／read access
- required incoming state
- guaranteed outgoing state

Composition作者が「これはUAV」と別に宣言する二重authorityは作らない。

ProducerとConsumerはkind、format、extent、rowBytes、mip／layer／sample／planeを完全一致させる。RGBA32F UAV producerをBGRA8 consumerへ接続するなどの不一致はContract段階で拒否する。

## 6. Verified Plan

PlannerはRGBA32F TextureをComposition-owned shared allocationとして固定する。

```text
logical bytes = rowBytes * height = width * 16 * height
```

State handoffは次を固定する。

```text
producer outgoing = UnorderedWrite
consumer incoming = PixelShaderRead または NonPixelShaderRead
```

独立Composition VerifierはContractからallocation、schedule、binding、handoff、signal、waitを再導出する。RuntimeがUAV barrier routeやconsumer stateを再選択しない。

初期版ではLeaf間handoffの前後に完全なresource transitionを置く。複数UAV writer、部分subresource transition、同一state内UAV ordering chainは対象外である。

## 7. D3D12物理写像

Composition-owned shared Textureのnative flagはFrozen initial stateから機械的に導く。

```text
RenderTarget state    -> D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
UnorderedWrite state  -> D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
ShaderRead state      -> D3D12_RESOURCE_FLAG_NONE
```

R32G32B32A32_FLOAT UAVには`D3D12_UNORDERED_ACCESS_VIEW_DESC`のTexture2D viewを作成する。Compute Root SignatureはSchema 17の`UnorderedAccessTable`を使用する。

Producer完了後、Composition Runtimeは既存のstate／completion handoff機構でshared TextureをUnorderedWriteからPixelShaderReadへ遷移し、同じnative resourceをconsumerへSRVとして渡す。

## 8. Readback

R32G32B32A32_FLOAT intermediate Textureも既存のpacked Texture readbackで観測できる。

```text
Canonical rowBytes = width * 16
D3D12 aligned RowPitch
  -> padding除去
Canonical packed RGBA float bytes
```

Consumer outputはBGRA8であり、従来どおりwidth * 4のpacked bytesを返す。D3D12 RowPitchはFrozen ABIへ保存しない。

## 9. Recovery

Whole-composition Recoveryでは、UAV-capable shared Texture、SRV／UAV descriptor、state、completionを新Device epochへ再物質化する。

```text
Controlled Recovery
  -> old Texture handle／completion失効
  -> External rebind acknowledgement
  -> RecoverySeed
  -> Compute UAV writer再実行
  -> SRV consumer再実行
  -> RGBA32F intermediate／BGRA8 output readback一致
```

Runtimeが失われたTexture内容を推測またはCPU復元することはない。

## 10. 資格試験

Architecture Gate:

- SGE4UNI 2.5 direct／round-trip／migration／corruption
- RGBA32F rowBytes = width * 16
- StorageTexture2D／UnorderedTexture2D semantic acceptance
- Shader Reflectionによるtyped Texture2D UAV照合
- UAV write endpointとSRV read endpoint
- UnorderedWrite → PixelShaderRead handoff
- RGBA32F producerとBGRA8 consumerのformat mismatch拒否
- ABI 1 migrationによるTexture推測拒否

Windows WARP Gate:

```text
4 x 4 Compute UAV producer
  -> 全pixelへ固定RGBA floatを書込
  -> RGBA32F intermediate readback一致
  -> Raster SRV consumer
  -> BGRA8 output readback一致
  -> Controlled Recovery
  -> RecoverySeed後も両readback一致
```

## 11. 非目標

Generalization 5は次を含まない。

- BGRA8 typed UAV
- arbitrary UAV format
- UAV BufferとTextureのalias
- multiple UAV writer
- UAV counter／append／consume
- GPU-generated mip chain
- mip／array／MSAA／multi-plane
- Depth／Stencil UAV
- partial subresource state
- same-state UAV barrier chain
- Texture aliasing
- Streaming／Residency
- cross-adapter Texture
- Temporal Texture Flow

これはTexture一般化ではない。Computeで生成した画像意味を、検証済みComposition ResourceとしてRaster／画像Observationへ接続する最小の完全経路である。

## 12. Windows資格で検出したInvocation rowBytes修正

初回Windows資格試験では、RGBA32F shared Textureの物質化とComposition Loadは成功したが、ExecutorのSubmit前検証がGeneralization 3由来のBGRA8固定条件`rowBytes == width * 4`を使用していたため、正しい`width * 16`のResourceを拒否した。

修正後はExternal Texture2D slotの`requiredFormat`から限定texel byte幅を導出し、BGRA8は4 bytes、RGBA32Fは16 bytesとしてNative Resourceのpacked rowBytesを照合する。未対応formatは引き続き拒否する。この修正はFrozen ABI、Composition Plan、UAV descriptor、state／completion handoff、readback、Recoveryを変更しない。
