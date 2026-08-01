# Source Authority Map V1

| Fact | Canonical owner | Planner role | Verifier role | Frozen owner | Runtime／Backend role |
|---|---|---|---|---|---|
| Leaf semantic graph | `LeafModel` | candidateを提案 | semantic／plan整合を独立再導出 | `LeafArtifact` | 再解釈しない |
| Leaf execution decision | `LeafModel` plan types | `LeafPlanner` | `LeafVerifier` | Schema 17 Package + `LeafCertificate` | Package通りに物質化 |
| Composition contract／flow | `CompositionModel` | schedule等を提案 | DAG、single writer、presenter等を独立検証 | flat `SGE4UNI 2.3` Composition Core + `CompositionCertificate` | scheduleを再計画しない |
| Texture2D Flow shape | 埋込みSchema 17 Leaf endpointから`CompositionModel`が再導出 | allocationを提案 | kind／format／extent／single writerを独立検証 | SGE4UNI 2.3 Contract／Plan schema 2 | shared Texture、state、completion、pitch写像を機械適用 |
| Dynamic membership／delta | `DynamicModelArtifact` input/history types | exact setを導出 | exact setを独立再導出 | `SGE4INV 1.3` | Frozen Invocationだけを消費 |
| Verified Dynamic payload route | `CompositionModel` Dynamic Contract | exact Update payloadをproposalへbind | payload set／identityを独立再導出 | `SGE4UNI 2.3` route + `SGE4INV 1.3` Execution Payload | verified Update／Clearをprivate shadowへ機械適用 |
| Device epoch／handles | `RuntimeCore` | なし | Runtime validation | Runtime Session state | D3D12 object lifetimeへ写像 |
| D3D12 target lowering | `D3D12Compiler` | Leaf compileのTarget stage | Package reader／runtime validation | Schema 17 sections | Executorは再loweringしない |
| D3D12 execution／recovery | `D3D12Executor` | なし | なし | Frozen Artifact／Runtime Sessionを参照 | mechanical execution／whole-composition recovery |
| Performance observations | Qualification evidence | Authorityなし | Correctness sealを発行しない | reference evidence | Runtime policyに使わない |

## Single-fact ownership rules

1. Planner proposalはVerified authorityではない。
2. Verifierだけが受理／拒否を確定する。
3. Frozen Artifactは検証済み完全判断を保存する。
4. Certificateは別の判断経路ではなく、完全Artifactから決定されるidentity束である。
5. RuntimeはRaw InvocationをCompileしない。
6. Backendは上流判断を推測・再計画しない。

7. Verified Dynamic SlotはCompositionとFrozen Invocationが所有し、Callerは同一Slotを上書きできない。

## Generalization 2 authority

| 事実 | Author | Planner | Independent Verifier | Frozen owner | Runtime |
|---|---|---|---|---|---|
| Conditional predicate／branch membership | Composition Dynamic Contract | 変更しない | graph closureとCanonical性を検査 | SGE4UNI 2.3 schema 3 | 再解釈しない |
| Region selection／enabled Leaf集合 | 直接指定不可 | exact setから導出 | exact setから独立再導出 | SGE4INV 1.3 Conditional Execution | Seal済み集合を機械適用 |


## Generalization 3 authority

| 事実 | Canonical owner | Verifier | Frozen owner | Executor |
|---|---|---|---|---|
| logical Texture extent／format | Composition Contract | Leaf endpointsとの完全一致を再検査 | SGE4UNI 2.3 Contract Data schema 2 | 再解釈しない |
| shared Texture allocation | Composition Planner | Contractと一致するpacked bytes／ownershipを再検査 | SGE4UNI 2.3 Verified Decision Data schema 2 | fixed D3D12 Textureへ物質化 |
| API row pitch | D3D12 Executor | Runtime binding shapeを検査 | Frozen ABIへ保存しない | upload／readback時だけ機械的に処理 |
