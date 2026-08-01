# Level 4 Generalization 5 — External Texture Invocation rowBytes修正

## 1. 発見された問題

Windows Full Gateで、限定Texture2D UAV FlowのComposition生成とLoadは成功したが、最初のSubmitが次で拒否された。

```text
invocation：Resourceが検証または実行の契約に違反しています。
```

拒否箇所はD3D12 ExecutorのExternal Texture2D binding検証だった。

## 2. 原因

Generalization 3ではExternal Texture2DがBGRA8に限定されていたため、Invocation検証がpacked rowBytesを次の式へ固定していた。

```text
rowBytes == width * 4
```

Generalization 5の中間Textureは`R32G32B32A32_FLOAT`であり、一texelは16 bytesである。Composition-owned shared Textureは正しく`width * 16`で物質化されていたため、Submit直前の旧BGRA8検証だけが正しいResourceを拒否していた。

## 3. 修正

External Texture2D binding検証で、Frozen Leafの`requiredFormat`から限定formatのtexel byte幅を導出する。

```text
B8G8R8A8_UNORM       -> 4 bytes
R32G32B32A32_FLOAT   -> 16 bytes
```

そして、Native ResourceのCanonical packed rowBytesを次と照合する。

```text
native rowBytes == expected width * bytesPerPixel(requiredFormat)
```

未対応formatは`bytesPerPixel == 0`となり、引き続き拒否される。

## 4. 変更しないもの

- SGE4UNI 2.5
- Leaf Schema 17
- Composition Contract／Plan
- Dynamic Invocation 1.4
- UAV descriptor生成
- Resource state／completion handoff
- packed upload／readback
- whole-composition Recovery

この修正はSubmit前のExternal Texture2D形状照合だけに限定される。
