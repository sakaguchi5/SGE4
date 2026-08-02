# Project Consolidation and Source Reconstruction Map

## v1.3.1

41製品プロジェクトを14責務プロジェクトへ統合し、Windows Full Gateを通過した。この段階ではActive sourceはまだ`src/internal/`にあり、旧完全実装、v2 identity authority、Unified Facadeが重なっていた。

## v1.4

Source authorityを見直し、15製品プロジェクトへ再構成した。Project数が14から15へ一つ増えた理由は、D3D12 CompilerとExecutorを正式に分離したためである。

| 最終プロジェクト | 所有責務 |
|---|---|
| `01_CanonicalCore` | Base、Digest、Identity、Canonical sectioned artifact |
| `10_LeafModel` | Semantic、Analysis、Compilation input、Execution Plan model |
| `11_LeafVerifier` | Leaf Planの独立検証とSeal |
| `12_LeafPlanner` | Raw Leaf Candidate proposal |
| `13_LeafArtifact` | Schema 17 Frozen Leaf reader／writer、D3D12 frozen schema／encoding、Leaf certificate |
| `20_CompositionModel` | Contract、Plan、Identity model |
| `21_CompositionPlanner` | Raw Composition Plan proposal |
| `22_CompositionVerifier` | Composition Planの独立検証とSeal。ArtifactのFreeze／Readは所有しない |
| `23_CompositionArtifactToolchain` | 平坦なSGE4UNI 2.8 freeze／read、certificate、ABI 1 migration、toolchain orchestration |
| `30_DynamicModelArtifact` | Dynamic input／decision／history／SGE4INV |
| `31_DynamicPlanner` | exact Dynamic proposal |
| `32_DynamicVerifier` | exact Dynamic independent verification |
| `40_RuntimeCore` | Package runtime、Runtime Session、epoch／handle／history authority |
| `50_D3D12Compiler` | Target profile、Verified Plan lowering、HLSL compile、Leaf compile orchestration |
| `51_D3D12Executor` | Device、materialization、submission、shared resource、readback、recovery |

## 維持した証明境界

- Leaf Planner／Leaf Verifier
- Composition Planner／Composition Verifier
- Dynamic Planner／Dynamic Verifier
- Portable Runtime Core／D3D12 Executor
- D3D12 Compiler／D3D12 Executor
- Product／Qualification

## 退役した過渡境界

- `src/internal/`
- 旧Level番号・R番号Source path
- Leafの第二v2 authority chain
- Compositionの第二v2 authority chain
- Runtime内部Dynamic Planning
- AliasだけのD3D12 Executor Facade

吸収した実装の来歴は`reference/retired_source/`へ保持する。

## D3D12 Artifact契約の配置

`D3D12Schema.h`と`D3D12Encoding.*`は、CompilerだけでなくExecutorも同じFrozen bytesを解釈するため、`13_LeafArtifact`に置く。`50_D3D12Compiler`はその契約へ書き込むLoweringを、`51_D3D12Executor`はその契約を機械的に読む実行を所有する。
