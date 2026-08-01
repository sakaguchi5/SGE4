# Source Authority Map V1

| Fact | Canonical owner | Planner role | Verifier role | Frozen owner | Runtime／Backend role |
|---|---|---|---|---|---|
| Leaf semantic graph | `LeafModel` | candidateを提案 | semantic／plan整合を独立再導出 | `LeafArtifact` | 再解釈しない |
| Leaf execution decision | `LeafModel` plan types | `LeafPlanner` | `LeafVerifier` | Schema 17 Package + `LeafCertificate` | Package通りに物質化 |
| Composition contract／flow | `CompositionModel` | schedule等を提案 | DAG、single writer、presenter等を独立検証 | flat `SGE4UNI 2.7` Composition Core + `CompositionCertificate` | scheduleを再計画しない |
| Texture2D Flow shape | 埋込みSchema 17 Leaf endpointから`CompositionModel`が再導出 | allocationを提案 | kind／format／extent／single writerを独立検証 | SGE4UNI 2.7 Contract／Plan schema 3 | shared Texture、state、completion、pitch写像を機械適用 |
| Texture2D UAV route | 埋込みSchema 17 Leafのformat／UAV view／UnorderedWrite stateから`CompositionModel`が再導出 | allocation／handoffを提案 | RGBA32F、single writer、UnorderedWrite→ShaderReadを独立検証 | SGE4UNI 2.7 Contract／Plan schema 3 | shared UAV Texture、descriptor、state、completionを機械適用 |
| Temporal Buffer lifetime／generation | Composition Contract schema 3 | same-frame DAGから分離したTemporal Planを提案 | lifetime／history depth／Current writer／Previous readers／二世代を独立再導出 | SGE4UNI 2.7 Contract／Plan schema 3 | Previous／Currentを別Resourceへbindし全Submit成功後だけ原子的にrotation |
| Dynamic membership／delta | `DynamicModelArtifact` input/history types | exact setを導出 | exact setを独立再導出 | `SGE4INV 1.5` | Frozen Invocationだけを消費 |
| Multi-target Verified Dynamic route | `CompositionModel` Dynamic Contract schema 5 | Canonical payloadと全routeをproposalへbind | route順序／slice／target／payload set／identityを独立再導出 | `SGE4UNI 2.7` route table + `SGE4INV 1.5` Execution Payload schema 2 | verified Update／Clearを全private shadowsへ機械適用し一括Commit |
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
| Conditional predicate／branch membership | Composition Dynamic Contract | 変更しない | graph closureとCanonical性を検査 | SGE4UNI 2.7 schema 3 | 再解釈しない |
| Region selection／enabled Leaf集合 | 直接指定不可 | exact setから導出 | exact setから独立再導出 | SGE4INV 1.5 Conditional Execution | Seal済み集合を機械適用 |


## Generalization 3 authority

| 事実 | Canonical owner | Verifier | Frozen owner | Executor |
|---|---|---|---|---|
| logical Texture extent／format | Composition Contract | Leaf endpointsとの完全一致を再検査 | SGE4UNI 2.7 Contract Data schema 3 | 再解釈しない |
| shared Texture allocation | Composition Planner | Contractと一致するpacked bytes／ownershipを再検査 | SGE4UNI 2.7 Verified Decision Data schema 3 | fixed D3D12 Textureへ物質化 |
| API row pitch | D3D12 Executor | Runtime binding shapeを検査 | Frozen ABIへ保存しない | upload／readback時だけ機械的に処理 |


## Generalization 4 authority

| 事実 | Canonical owner | Planner | Independent Verifier | Frozen owner | Runtime／Executor |
|---|---|---|---|---|---|
| Indirect target／maximum | Composition Dynamic Contract | 変更しない | Schema 17 Command／operationとの一致を検査 | SGE4UNI 2.7 Dynamic Contract schema 5 | 再選択しない |
| exact work count | exact Transition set | countとX／Y／Zを提案 | exact setから独立再導出 | SGE4INV 1.5 Indirect Dispatch | 再計算／clampしない |
| native indirect argument bytes | Seal済みDispatch引数 | なし | Runtime identity／上限検査 | Frozen引数から導出 | frame-slot Bufferへ機械的に書込み |
| dispatch operation substitution | Schema 17 ExecuteCompute target | なし | targetが一意であることをComposition境界で検査 | Composition route + Leaf operation | 対象だけExecuteIndirectへ置換 |
