# Frozen Artifact Hierarchy Contract — ABI 2.0 Revision

この文書名は既存参照を保つためV1を維持するが、内容はFrozen Composition ABI 2.0の正本構造を表す。

## Frozen Leaf

既存Schema 17の完全な実行成果物を維持する。Shader binary、Resource、Allocation、View、Binding、Executable、Operation stream、Invocation schemaを保持する。

`LeafCertificate`は完全PackageとD3D12 Package Viewから直接生成され、次をbindする。

- Semantic identity
- Package execution identity
- Frozen artifact identity
- Target profile identity
- Resource contract identity
- Write set identity
- Operation sequence identity
- Verification seal identity

Certificate生成のために第二Planner／Verifierを実行してはならない。

## Frozen Composition

Production Frozen Compositionは平坦な`SGE4UNI 2.8`である。

```text
SGE4UNI 2.8
  Manifest
  Leaf Table
  complete Schema 17 Leaf Package bytes
  Contract Data
  Verified Decision Data
  Verification Certificate
  Authority Ledger
  Dynamic Contract
```

`SGE4CMP 1.0`を内包する`CompleteComposition` Sectionは存在しない。Leaf Packageだけが独立した下位Artifactとして保持される。

`CompositionCertificate`は、ABI 2.8 Composition Core digestと検証済みContract／Plan／Sealから直接生成され、次をbindする。

- Frozen Composition identity
- Contract identity
- Plan identity
- Seal identity
- Schedule identity
- Recovery set identity
- Leaf／Flow count

ABI 1.1／SGE4CMP 1.0はmigration資格試験に限定し、Production Readerから参照しない。

## Frozen Dynamic Invocation

`SGE4INV` major 1／minor 6は、正確な集合、Transition record、前History identity、次History、Sealとidentityを保存する。RuntimeがCountだけからMembershipを推測すること、および受理済みHistoryとは異なるHistoryから作られたContinueHistory成果物をSubmitすることを禁止する。

生成経路は一つである。

```text
Request -> Dynamic Planner -> Independent Verifier -> Freeze
```

## Digest layers

- embedded Schema 17 Leaf execution／file digest
- Leaf Certificate identities
- ABI 2.0 Composition Core digest
- Composition Certificate identities
- outer SGE4UNI semantic／file digest
- Frozen Dynamic Invocation identity／history identity

Composition Readerは外側Section、Leaf Table、各Leaf Package、Contract、Plan、Verifier Certificate、Authority Ledger、Dynamic Contractを物質化前に検証する。どれか一つでも不一致なら拒否する。


## Generalization 3 amendment

SGE4UNI 2.8はContract Data／Verified Decision Data schema 3へfixed Texture2D shapeを保存する。Leaf Schema 17とSGE4INV 1.6は変更しない。TextureのAPI row pitchやnative descriptor handleはFrozen階層へ混入させず、D3D12 Executorの物理写像に限定する。


## Generalization 4

SGE4UNI 2.8 Dynamic Contract schema 6はVerified Indirect Dispatch routeを所有する。SGE4INV 1.6 Indirect Dispatch Sectionは独立Verifierが確定したworkCount、Dispatch引数、identityを所有する。Leaf Schema 17はstatic maximum Compute Commandを保持し、Executorが対象operationだけをIndirect実行へ機械的に写像する。

## Generalization 5追加境界

SGE4UNI 2.8はContract／Plan schema 3で、R32G32B32A32_FLOAT Texture2D、UnorderedWrite producer、ShaderRead consumerを既存format／shape／state recordで表現する。Leaf Schema 17がUAV viewを所有し、Compositionはその事実を再導出する。D3D12 UAV descriptor、native flag、aligned RowPitchはExecutorだけが所有する。

## Generalization 6 multi-target dynamic hierarchy

SGE4UNI 2.8 Dynamic Contract schema 6はCanonical member byte幅とCanonical route tableを所有する。SGE4INV 1.6 Execution Payload schema 2は同じroute table、Canonical Update payload、payload identityを所有する。Runtimeはrouteごとのprivate shadowを持つが、その全体を一つのHistory acceptance unitとして扱う。


## Generalization 7 temporal hierarchy

SGE4UNI 2.8 Contract Data／Verified Decision Data schema 3はResource lifetime、history depth、physical instance count、Current writer、Previous readersを所有する。Temporal Bufferはsame-frame handoff／signal／waitから分離され、専用Temporal Planだけに現れる。Runtimeは二つのnative BufferをPrevious／Currentとして所有し、全Leaf Submit成功後だけrole indexを原子的に交換する。SGE4INV 1.6とLeaf Schema 17は変更しない。


## Generalization 8 compact worklist hierarchy

SGE4UNI 2.8 Dynamic Contract schema 6はVerifiedU32 Compact Worklist modeとtarget index-list Dynamic Slotを所有する。SGE4INV 1.6 Compact Worklist Sectionはexact Transition setから独立再導出されたCanonical uint32 member ID列とidentityを所有する。Runtimeが生成するfixed-size Slot bytesはFrozen listの機械的materializationであり、別authorityではない。
