# Level 4 v1 Canonical R1 Evidence Map

R1はR0 Proof LedgerのうちFrozen Artifact境界に関係する項目を、次の範囲で具体化する。

| Proof ID | R1 evidence | R1 status |
|---|---|---|
| ARTIFACT-001 | `FrozenCompositionWriter`と`FrozenCompositionReader`はSource、Compiler、Planner、Runtime、Backendを参照しない。Embedded Leaf、Contract bytes、Verified Decision bytes、Certificate bytesを一つのfileへ固定する。 | Boundary proven; R2でContract/Certificate authorityを追加 |
| ARTIFACT-002 | Header、固定六Section、Section digest、Embedded Leaf Package、Semantic/Decision/Certificate/File digestを物質化前に検証する。 | Structural portion proven |
| ARTIFACT-003 | bad magic、raw corruption、Section順序、Presenter、Leaf offset、Embedded Leaf、Contract digest、padding corruptionを拒否する。 | R1 proven |
| ARTIFACT-004 | Fresh Debug A/B、Debug/Release、declaration order、round tripのbyte identityを要求する。 | Windows qualification pending until runner passes |
| REGRESSION-001 | Embedded executable LeafをD3D12 Schema 17 / Runtime 17に限定する。 | R1 proven |
| ARCH-001 | Artifact ProjectはFoundationとFrozenPackageCoreだけを参照する。 | R1 boundary script |
| ARCH-002 | Prepare時にdependency、script contract、SOURCE_MANIFESTを検証する。 | R1 prepare gate |

## R1で未成立のProof ID

次はR2以降の責務であり、R1のopaque Sectionが存在するだけでは成立扱いにしない。

- ABI-001〜004
- CONTRACT-001〜007
- PLAN-001〜003
- VERIFY-001〜004
- RUNTIME-001〜007
- RECOVERY-001〜009

特に`VerifiedDecisionData`というSection名は、格納されたbytesが本当に独立Verifierにより認定済みであることをR1単独では意味しない。
R2でprivate Verified type、独立再導出、Certificate生成とReader検査が接続された時点で初めてVerifier authorityを主張する。
