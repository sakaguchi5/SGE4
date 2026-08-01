# Level 4 Generalization 3 修正報告：SampledTexture Static Sampler契約

## 1. 症状

Windows統合設計試験で限定Texture2D Compositionを通常のSemantic Compilerから生成した際、
`shader-reflection`段階で次の拒否が発生した。

```text
Textureが検証または実行の契約に違反しています。
```

Semantic Analysisは通過しており、Pixel ShaderのTexture SRV自体もReflectionできていた。
拒否は、Program Interfaceが`SampledTexture`を宣言しているのに、Shader Reflection上のSamplerが0件だったためである。

## 2. 原因

New SGE4の`SampledTexture`契約は、次を一体として固定する。

```text
Texture2D SRV : tN
point-clamp Static Sampler : s0
```

Target Compilerは`SampledTexture`を検出すると、Root Signatureへs0のpoint-clamp Static Samplerを固定し、
Shader ReflectionでもPixel Shaderがs0を1件使用していることを要求する。

一方、Generalization 3のWindows資格Fixtureは次のShaderを使用していた。

```hlsl
Texture2D<float4> InputTexture : register(t0);
return InputTexture.Load(...);
```

`Load`はSamplerを使用しないため、D3DCompile後のReflectionではSamplerが存在せず、
正本の`SampledTexture`契約に違反していた。

## 3. 修正

Compiler契約やFrozen ABIは緩めず、資格Fixtureを契約どおりのSampledTexture Shaderへ修正した。

```hlsl
Texture2D<float4> InputTexture : register(t0);
SamplerState InputSampler : register(s0);
return InputTexture.SampleLevel(InputSampler, uv, 0.0f);
```

Root Signatureが既に固定しているpoint-clamp samplerを使用する。
Producer Textureは全画素が同じ固定色であるため、Samplingへの変更によって期待pixel bytesは変化しない。

## 4. 変更しないもの

- Semantic Modelの`SampledTexture`の意味
- Shader Reflection検証規則
- Static Sampler生成規則
- SGE4UNI 2.3
- Leaf Schema 17
- Composition Planner／Verifier
- Texture2D Flowの形状・state・Recovery契約
- packed readbackの期待値

## 5. 再発防止

Texture consumer資格Fixture自身が、正本Program Interfaceと同じbinding集合をHLSL上で実使用する。
Samplerを宣言するだけで未使用にすると最適化でReflectionから除去され得るため、`SampleLevel`で明示的に使用する。
