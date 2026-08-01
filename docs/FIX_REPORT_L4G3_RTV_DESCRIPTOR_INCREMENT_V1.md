# Level 4 Generalization 3 — RTV descriptor increment修正報告

## 1. 現象

Windows／MSVC Debug buildで、`ExecutorInstance.inl`のExternal Texture2D RTV生成経路が次の未定義識別子によりコンパイル失敗した。

```text
error C2065: 'rtvDescriptorIncrement_': 定義されていない識別子です。
```

## 2. 原因

D3D12 Executorが正式に所有しているRTV descriptor increment memberは`rtvIncrement_`である。

Generalization 3で追加したExternal Texture2D RenderTarget view生成経路だけが、存在しない`rtvDescriptorIncrement_`を参照していた。Shader descriptor側の`shaderDescriptorIncrement_`と命名を混同した単純な識別子誤りであり、Frozen ABI、Composition契約、Texture shape、state handoffの意味には影響しない。

## 3. 修正

```text
rtvDescriptorIncrement_
    ↓
rtvIncrement_
```

既存のRTV heap生成、Package-owned RTV生成、Raster実行時のRTV handle計算と同じmemberを使用する。

## 4. 影響範囲

- 変更対象はD3D12 ExecutorのExternal Texture2D RTV descriptor address計算だけである。
- `SGE4UNI 2.3`、Schema 17、`SGE4INV 1.3`のbytesや意味は変更しない。
- Planner、Verifier、Runtime Session、Recovery契約は変更しない。
- Windows Full Gateの再実行で最終確認する。
