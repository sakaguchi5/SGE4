# Source Authority Map V1

| Fact | Canonical owner | Planner role | Verifier role | Frozen owner | Runtime／Backend role |
|---|---|---|---|---|---|
| Leaf semantic graph | `LeafModel` | candidateを提案 | semantic／plan整合を独立再導出 | `LeafArtifact` | 再解釈しない |
| Leaf execution decision | `LeafModel` plan types | `LeafPlanner` | `LeafVerifier` | Schema 17 Package + `LeafCertificate` | Package通りに物質化 |
| Composition contract／flow | `CompositionModel` | schedule等を提案 | DAG、single writer、presenter等を独立検証 | flat `SGE4UNI 2.1` Composition Core + `CompositionCertificate` | scheduleを再計画しない |
| Dynamic membership／delta | `DynamicModelArtifact` input/history types | exact setを導出 | exact setを独立再導出 | `SGE4INV 1.2` | Frozen Invocationだけを消費 |
| Verified Dynamic payload route | `CompositionModel` Dynamic Contract | exact Update payloadをproposalへbind | payload set／identityを独立再導出 | `SGE4UNI 2.1` route + `SGE4INV 1.2` Execution Payload | verified Update／Clearをprivate shadowへ機械適用 |
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
