> Current status: Generalization 7により現行ProductionはSGE4UNI 2.7／SGE4INV 1.5へ進んだ。本書の2.3記述はGeneralization 3完成時点の契約記録であり、限定Texture2D Flowの意味は維持される。

# Level 4 Generalization 3 — Limited Texture2D Flow

## 1. 目的

Generalization 3は、Composition Resource FlowをBuffer-onlyから、実証可能な最小Texture2Dへ拡張する。

対象は次の固定形だけである。

```text
Resource kind       Texture2D
Format              B8G8R8A8_UNORM
Extent              fixed width / fixed height
Mip                 1
Array layer         1
Plane               1
Sample count        1
Writer               exactly one
Lifetime relation   same-frame static DAG
Producer view       RenderTarget
Consumer view       ShaderResource
```

Textureの汎用化、subresource graph、UAV Texture、Depth共有、MSAA、mip chain、array、Streaming、Residencyは導入しない。

## 2. Frozen成果物

Production Frozen Compositionは`SGE4UNI 2.3`である。

```text
SGE4UNI 2.3
  Manifest schema 2
  Leaf Table schema 1
  complete Schema 17 Leaf bytes
  Contract Data schema 2
  Verified Decision Data schema 2
  Verification Certificate schema 1
  Authority Ledger schema 2
  Dynamic Contract schema 3
```

Contract Data schema 2はEndpointとResource Flowへ次を固定する。

```text
width
height
rowBytes
mipLevels
arrayLayers
sampleCount
planeCount
format
```

Verified Decision Data schema 2はAllocation Planへ同じTexture形状とpacked logical bytesを固定する。

```text
packed logical bytes = rowBytes * height
```

D3D12固有のupload/readback row pitchはFrozen Compositionの意味ではない。ExecutorがAPI要求に従って物理pitchへ写像し、Readback時にpaddingを除去してpacked bytesへ戻す。

Leaf Package ABIはSchema 17を維持する。Frozen Dynamic Invocationは`SGE4INV 1.3`を維持する。

## 3. Composition authority

Composition Modelは、埋込みLeaf PackageのExternal Texture2D slotから形状を再導出する。作者がComposition側へ別のTexture形状を二重宣言することはない。

ProducerとConsumerは次を完全一致させなければならない。

```text
ResourceKind
Format
width / height
rowBytes
mip / layer / sample / plane
```

不一致はComposition Plannerへ進む前に拒否する。

Flowは従来どおりsingle writerであり、producerとconsumerを結ぶ静的DAGである。Texture用の別scheduleやRuntime graphは作らない。

## 4. PlanとRuntime

Composition PlannerはTexture2D Flowごとに一つのComposition-owned shared Textureを計画する。

RuntimeはFrozen Planを再判断せず、次を機械的に行う。

```text
1. fixed Texture2Dをshared DeviceDomainへ物質化
2. producer incoming stateへ遷移
3. producer LeafへRTVとしてbind
4. producer release completionを受理
5. consumer incoming stateへ遷移
6. consumer LeafへSRVとしてbind
7. consumer release completionを受理
8. 必要時にpacked Texture readback
```

同じshared Texture objectをLeaf間で受け渡す。CPUによる中間copyやTexture内容の再解釈は行わない。

## 5. D3D12写像

Executorはshared Textureを`D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET`付きのfixed Texture2Dとして作成する。SRVを禁止するflagは付けないため、同じResourceをproducerではRTV、consumerではSRVとして利用できる。

初期化とReadbackでは`GetCopyableFootprints`を使用する。

```text
Canonical packed rowBytes
  -> D3D12 aligned RowPitchへrow単位copy
  -> GPU Texture
  -> D3D12 readback RowPitch
  -> padding除去
  -> Canonical packed rowBytes
```

Raster LeafがSurfaceを持たずExternal fixed TextureをColor Attachmentとする場合、viewportとscissorはFrozen Resource extentから決める。Window sizeから推測しない。

## 6. Conditional Regionとの関係

Generalization 2のConditional Regionは維持される。Texture producer／consumerをConditional Regionへ所属させる場合も、既存のgraph closure規則が適用される。

未選択Texture outputについてRuntimeはfallback生成、暗黙Clear、再描画を行わない。直前に受理されたResource stateとcompletionを保持する。

## 7. Recovery

Whole-composition Recoveryではshared Textureを新Device epochへ再物質化する。External Leaf bindingは新しいTexture handleとcompletionへ張り直される。

資格試験では次を確認する。

```text
4 x 4 producer Texture
  -> fullscreen Rasterで固定色を書込
  -> Texture Flow
  -> consumer Pixel ShaderがTexture2D.Load
  -> Composition Output Texture
  -> packed readback一致
  -> controlled whole-composition recovery
  -> 再実行後もpacked readback一致
```

## 8. ABI 1移行

SGE4UNI 1.1／SGE4CMP 1.0はBuffer Flowしか表現しない。MigratorはBuffer-only corpusだけを`SGE4UNI 2.3`へ移行する。

Texture2D FlowをABI 1形式へ逆算したり、旧bytesからTexture shapeを推測したりしてはならない。Textureを含む入力はABI 1 migration writerが明示拒否する。

## 9. Negative Gate

最低限、次を拒否する。

- producer／consumerのwidthまたはheight不一致
- format不一致
- rowBytes不一致
- mip、array、sample、planeが限定値以外
- Texture2DにBuffer viewを接続
- BufferにTexture viewを接続
- Texture RenderTarget以外のwrite endpoint
- Texture ShaderResource以外のread endpoint
- multiple writer
- ABI 1 migrationによるTexture推測
- Frozen ContractまたはPlan内のTexture shape改竄
- Runtime bindingのkind、format、extent不一致

## 10. 今回含めないもの

- Texture1D／Texture3D
- mip chain
- Texture array
- multi-plane format
- MSAA
- Depth／Stencil Flow
- UAV Texture Flow
- Copy-only Texture Flow
- partial subresource transition
- surface-relative shared Texture
- Streaming／Residency
- cross-adapter Texture
- Texture aliasing

Generalization 3はTexture一般論ではない。Level 5の画像ObservationやRaster／Compute表現比較を載せられる、最小で完全なsame-frame Texture2D Flowを正本化する段階である。
