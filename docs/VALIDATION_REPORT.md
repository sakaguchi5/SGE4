# Validation Addendum — Level 4 Generalization 2

## Production formats

```text
Frozen Composition: SGE4UNI 2.4
Dynamic Contract: schema 3
Frozen Dynamic Invocation: SGE4INV 1.4
Invocation Manifest: schema 4
Execution Payload Section: schema 1
```

## Implemented evidence

- Composition freezes `AuthorityOnly` or one `VerifiedDenseSlot` route.
- Writer／Reader decode the target Schema 17 Leaf and prove `slot.requiredBytes == universeCount * memberBytes`.
- Dynamic Planner and independent Verifier require the execution payload member set to equal the exact Update set.
- Payload identity is included in Invocation identity, Verifier seal, Frozen Invocation identity, Manifest, and Execution Payload Section.
- Runtime applies verified Update／Clear records to a private dense shadow and commits shadow／History only after native submission succeeds.
- Caller collision on the verified Leaf／Slot is rejected.
- Recovery clears the shadow and requires a full-active RecoverySeed.

## Validation completed in this environment

- `tools/static_audit.py`: passed; 18 projects, 15 product projects, 47 active translation units, 40 carried invariants.
- C++23 strict syntax checks passed for all modified portable Product sources.
- C++23 strict syntax checks passed for Architecture tests, ABI corruption tests, D3D12 Runtime facade, and Windows qualification source.
- Project／filters ownership for the new `DynamicExecutionContract.h` passed.

## Conditional shadow commit correction

- Windows qualification detected that zero-Leaf false branch advanced History without committing the prepared verified dense shadow.
- `hasBinding` is now used only for native Dynamic Slot injection.
- A successful `VerifiedDenseSlot` submission commits prepared shadow and History together even when zero Leaves execute.
- Architecture regression covers InitialSeed -> zero-Leaf Clear -> re-enable and checks that only the newly active member remains in the prepared dense shadow.
- Frozen ABI, Planner, Verifier, and Conditional Region identities are unchanged.

## Windows qualification still required

The patch has not been claimed as MSVC／HLSL／WARP qualified in this environment. Run:

```bat
run_new_sge4_full_gate.bat
```

The updated Windows qualification additionally checks InitialSeed, ContinueHistory Update／Retain／Clear, zero-transition retention, caller collision rejection, payload omission rejection, and RecoverySeed rematerialization against GPU readback.

---

# Historical Validation Report — Frozen Composition ABI 2.0


## Baseline evidence

`NewSGE4 v1.5.2`はユーザーのWindows環境で次を含むFull Gateを通過した変更不能Oracleである。

- MSVC Debug／Release build
- C++23 Source reconstruction
- ABI 1.x Golden bytes
- Architecture／Migration
- WARP qualification
- Controlled whole-Composition Recovery
- Actual Device removal
- MSBuild正常終了

ABI 2.0はこのBaselineのComposition意味、Schema 17 Leaf bytes、Planner／Verifier境界、Dynamic Invocation ABI、Runtime観測結果、Recovery意味を維持し、Frozen Composition containerだけを平坦化する。

## ABI変更

```text
変更前
  SGE4UNI 1.1
    CompleteComposition
      SGE4CMP 1.0
        Schema 17 Leaf bytes
        Contract
        Verified Decision
        Verification Certificate
    Authority Ledger
    Dynamic Contract

変更後
  SGE4UNI 2.0
    Manifest
    Leaf Table
    Schema 17 Leaf bytes
    Contract Data
    Verified Decision Data
    Verification Certificate
    Authority Ledger
    Dynamic Contract
```

維持:

```text
Frozen Leaf Target Schema: 17
Frozen Leaf Minimum Runtime: 17
Frozen Dynamic Invocation: SGE4INV 1.1
Project boundary: 15 Product + 3 Qualification
Carried invariants: 40
```

## Source変更

- `src/composition/artifact/abi2/FrozenCompositionAbi2.*`を追加
- Production `VerifiedCompositionArtifact`をABI 2.0専用Readerへ変更
- `CompositionToolchain`を平坦な8 Section Writer／Readerへ変更
- Composition Certificate schema 2を導入
- Composition Core digestを導入
- Runtime／D3D12 Runtimeへ外側SGE4UNI 2.0 bytesを渡すよう変更
- ABI 1.1／SGE4CMP Reader／Writerを`src/composition/migration/abi1/`へ隔離
- Plannerを再実行しない明示的v1.1 -> v2.0 Migratorを追加
- ABI 2.0 Portable Round-trip／Migration self-testを追加
- ABI 2.0 corruption corpusを追加
- Generic Sectioned Artifact Readerにzero-padding検証を追加
- Schema 17 Package Readerのzero-size Section overlap判定を修正

## この環境で実施した検証

### Static audit

```text
Project数: 18
Product Project数: 15
Active Product Translation Unit数: 47
Carried invariant数: 40
ProjectReference DAG: 非循環
```

追加監査:

- Production ABIは`SGE4UNI 2.0`に固定
- exact 8 direct Section
- Productionに`CompleteComposition`なし
- Production artifact treeにlegacy SGE4CMP containerなし
- legacy codeは`migration/abi1`だけに存在
- Production Readerはlegacy Reader／Writerを参照しない
- Schema 17／Runtime 17を維持
- SGE4INV 1.1を維持
- ABI 2.0 corruption／portable self-testが存在

### Portable C++23 strict syntax

Windows専用の次の3 Translation Unitを除き、Product 45本、Test／Fixture 6本、合計51本を検査した。

```text
-std=c++23
-Wall
-Wextra
-Wpedantic
-Werror
```

Windows専用:

```text
src/backends/d3d12/compiler/lowering/D3D12PackageLowering.cpp
src/backends/d3d12/executor/Executor.cpp
tests/61_UnifiedWindowsQualification/main.cpp
```

結果:

```text
strict syntax passed: 51
```

### ABI 1.x Oracle

`Abi1GoldenBytes`を実行し、次のv1.5.2以前の固定SHA-256を維持した。

```text
Sectioned Artifact
Frozen Leaf Package A
Frozen Leaf Package B
SGE4CMP 1.0 Frozen Composition
```

結果:

```text
ABI 1.x golden bytes passed.
```

### ABI 2.0 Portable self-test

D3D12 API objectを作らず、D3D12 Schema 17として完全に検証可能な外部Buffer二本のPortable Leaf Packageを生成した。同じLeaf二個から、Composition input -> internal -> outputの三Flowを構築した。

確認項目:

- Schema 17 Leaf Package build／read／D3D12 schema decode
- SGE4UNI 2.0を二回直接生成してbyte一致
- Portable Golden file bytes = 9304
- Portable Golden SHA-256 = `753c82dfc62c65cf56d09bccf81b9b081b2e9c9a04c81ddce2d0b79f36b77223`
- major 2／minor 0
- exact 8 direct Section
- 埋め込みLeaf bytesの完全一致
- ABI 2.0 Round-trip
- Core／Semantic／File digest一致
- SGE4UNI 1.1 Corpus生成
- Production ReaderによるABI 1.1拒否
- 明示的Migration ToolによるABI 1.1 -> 2.0変換
- 直接生成v2とMigration後v2のbyte完全一致
- Contract／Plan／Seal／Schedule／Recovery Set identity保存
- corruption corpus

Debug相当`-O0`とRelease相当`-O2 -DNDEBUG`の両方で実行した。

結果:

```text
ABI 2.0 portable self-test passed.
```

### ABI 2.0 corruption corpus

Reader 16ケースとWriter 1ケース、合計17のNegative Gateを実行した。

- 必須Section欠落
- 未知Required Section
- Manifest schema不一致
- 非dense Leaf ID
- 非Canonical Leaf offset
- 非zero Leaf padding
- Leaf Package破損
- Contract破損
- Verified Decision破損
- Verification Certificate破損
- Composition Core digest不一致
- Frozen Composition identity不一致
- Authority Ledger不一致
- Dynamic identity不一致
- descriptor offset範囲外
- descriptor kind重複
- Writerへの重複Section入力

### Migration Acceptance

```text
New SGE4統合移行受入試験に合格
再現した不変条件数: 40
```

## Windowsで必要な最終確認

Frozen Composition file bytes、Reader経路、Runtime load入力、資格試験の実Leaf経路を変更したため、次はWindows／MSVC／D3D12でのみ確定する。

```bat
run_new_sge4_full_gate.bat
```

- MSVC v145 Debug／Release build
- ABI 2.0 Sourceの全Static Library Link
- HLSL compile
- 実Schema 17 Frozen Leaf生成
- flat SGE4UNI 2.0生成
- SGE4INV 1.1生成
- ABI 1.x Oracle
- ABI 2.0 direct／migration／corruption
- Debug A／Debug B／Release SGE4UNI 2.0 byte一致
- WARP materialization／submission／readback
- Controlled whole-Composition Recovery
- stale epoch rejection
- Actual Device removal／removed-adapter exclusion
- Gate終了後のMSBuild node終了

このLinux環境では、上記Windows固有結果を通過済みとは記録しない。

## Level 4 Generalization 2 validation target

- SGE4UNI 2.4／Dynamic Contract schema 3
- SGE4INV 1.4／Conditional Execution Section
- True／False branchの独立再導出
- enabled Leaf集合改竄拒否
- zero-Leaf submissionとResource状態保持
- 再有効化時のCommit済みshadow反映
- Controlled Recovery後のRecoverySeed再構築

Linux側ではC++23厳格構文検査、静的Architecture監査、Manifest照合を実施する。MSVC、HLSL、WARP、Actual Device removalの最終合格はWindows Full Gateで確定する。


## Level 4 Generalization 3 validation target

- SGE4UNI 2.4
- Contract Data schema 2／Verified Decision Data schema 2
- fixed B8G8R8A8_UNORM Texture2D shape
- single mip／layer／plane／sample
- RTV producer -> SRV consumer state／completion handoff
- D3D12 pitch-aware initialization and packed readback
- producer／consumer extent mismatch rejection
- ABI 1 migration Texture rejection
- controlled whole-composition Recovery後のTexture再物質化

Linux側ではABI 1 golden bytes、SGE4UNI 2.4 direct／migration／round-trip／corruption、portable Texture Composition、C++23厳格構文検査、静的Architecture監査、Manifest照合を実施する。MSVC、HLSL、WARP、Actual Device removalの最終合格はWindows Full Gateで確定する。

## Level 4 Generalization 3 — RTV descriptor increment build fix

Windows／MSVC buildで、External Texture2D RTV生成経路が存在しない`rtvDescriptorIncrement_`を参照していたことを確認した。正式memberである`rtvIncrement_`へ修正した。

この修正はdescriptor handleの物理address計算だけに限定され、Frozen ABI、Composition／Dynamic authority、Texture2D Flow契約には変更を加えない。Windows Full Gateを再実行してMSVC、HLSL、WARP、Recoveryを確認する。

## Level 4 Generalization 3 — Texture2D Flow Runtime型alias build fix

Windows／MSVC buildで、D3D12 Runtimeの型集約Headerに`Texture2DFlowShape` aliasが欠落していたことを確認した。正式所有者`::sge4::composition::Texture2DFlowShape`を`runtime_detail`へaliasし、`CompositionSharedResources.h`の型解決を修正した。

この修正はC++の名前解決境界だけに限定される。Frozen ABI、Composition Plan、Texture2D Flow形状、D3D12物理Resource、state／completion handoff、readback、Recoveryの意味は変更しない。Windows Full Gateを再実行してMSVC、HLSL、WARP、Recoveryを確認する。

## Level 4 Generalization 3 — Offscreen Raster Semantic Contract fix

Windows統合設計試験で、限定Texture2D producer／consumer Leafが`semantic-analysis`により拒否された。`ColorAttachment`のResourceUse境界は固定External Texture2Dを許可していたが、Raster Work境界にPresentation専用の`PresentSource == 1`条件が残っていた。

Raster契約を次の二形態へ分離した。

- SurfaceImage ColorAttachmentは、同一ResourceのPresentSourceを必須とする。
- fixed External Texture2D ColorAttachmentは、PresentSourceを禁止する。

Portable Semantic Analysis回帰試験で、offscreen Texture Rasterと従来Surface Rasterを受理し、PresentSourceを欠くSurface Rasterを拒否することを確認した。Frozen ABI、Composition Plan、Texture物質化、state／completion handoffには変更を加えていない。

## Level 4 Generalization 3 SampledTexture Static Sampler修正

- Windows資格FixtureのTexture consumerを、正本`SampledTexture`契約どおり`t0` SRV＋`s0` Static Samplerへ修正した。
- `Texture2D.Load`を`SampleLevel`へ変更し、D3DCompile最適化後もSamplerがReflectionへ残るようにした。
- Compiler、Frozen ABI、Composition契約、Runtime、期待packed pixel bytesは変更していない。
- Portable C++23 strict syntax、静的Architecture監査、SOURCE_MANIFEST照合を再実行した。


## Level 4 Generalization 4 validation target

- SGE4UNI 2.4／Dynamic Contract schema 4
- SGE4INV 1.4／Manifest schema 5／Indirect Dispatch Section
- exact Transition countからworkCount／Dispatch Xを独立導出
- zero-work DispatchIndirect
- target route／maximum／identity改竄拒否
- target Compute Commandの一意なoperation適用
- fixed Dispatchとの観測意味一致
- Controlled Recovery後のCommand Signature／argument buffer再物質化
- RecoverySeedによるwork count再構築

Linux側ではABI 1 golden bytes、SGE4UNI 2.4 authority-only direct／migration／round-trip／corruption、C++23厳格構文検査、静的Architecture監査、Manifest照合を実施する。MSVC、HLSL、WARP、D3D12 ExecuteIndirect、Actual Device removalの最終合格はWindows Full Gateで確定する。

## Level 4 Generalization 4 — None Indirect Work Count検証修正

Windows統合設計試験で、Indirect契約を持たない既存CompositionのInvocationが独立Verifierに拒否された。原因は、Dynamic algebra上の`indirectWorkCount`と、Generalization 4で追加したGPU Dispatch用`VerifiedIndirectDispatchV1::workCount`をmodeに関係なく一致させていたことである。

`VerifiedDispatch`の場合だけ両者を一致させ、`None`の場合は既存どおりtransition countをDynamic Decisionへ保持しつつ、Dispatch引数をzeroに固定するよう修正した。AuthorityOnly回帰試験ではtransition count 3とDispatch workCount 0の同時成立を確認する。

## L4G4 AuthorityOnly transition audit fix

- `AuthorityOnly + VerifiedDispatch`でexact transition countとshadow適用数を分離した。
- Runtime Session回帰: verified=3、applied=0、DispatchIndirect work=3。
- Windows資格失敗時にD3D12 Submitのstage／messageを表示する。
